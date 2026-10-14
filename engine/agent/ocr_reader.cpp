#include "ocr_reader.h"

#include <cpp-httplib/httplib.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace delta {
namespace agent {

const std::vector<OcrModel>& ocr_models() {
    // Smallest first. Unlimited-OCR runs page by page until llama.cpp gains its R-SWA attention.
    static const std::vector<OcrModel> models = {
        {"glm-ocr",
         "GLM-OCR",
         "ggml-org/GLM-OCR-GGUF",
         "GLM-OCR-Q8_0.gguf",
         "mmproj-GLM-OCR-Q8_0.gguf",
         950433408LL + 484403648LL,
         0,
         "Text Recognition:",
         {"-c", "8192", "--temp", "0"}},
        {"unlimited-ocr",
         "Unlimited-OCR",
         "sabafallah/Unlimited-OCR-GGUF",
         "unlimited-ocr-Q4_K_M.gguf",
         "mmproj-unlimited-ocr-q8_0.gguf",
         1950326592LL + 461226464LL,
         16,
         "document parsing.",
         {"-c", "16384", "--chat-template", "deepseek-ocr", "--no-jinja", "--flash-attn", "off", "--temp", "0",
          "--dry-multiplier", "0.8", "--dry-base", "1.75", "--dry-allowed-length", "35", "--dry-penalty-last-n", "128",
          "--dry-sequence-breaker", "none"}},
    };
    return models;
}

const OcrModel& pick_ocr_model(int ram_gb, const std::function<bool(const OcrModel&)>& installed) {
    const auto& models = ocr_models();
    for (auto it = models.rbegin(); it != models.rend(); ++it)
        if (installed && installed(*it))
            return *it;
    for (auto it = models.rbegin(); it != models.rend(); ++it)
        if (ram_gb > 0 && ram_gb >= it->min_ram_gb)
            return *it;
    return models.front();
}

std::string clean_ocr_text(const std::string& raw) {
    // DeepSeek-style layout markup: <|ref|>type<|/ref|><|det|>[[x,y,x,y]]<|/det|>.
    static const std::regex boxes(R"(<\|(ref|det)\|>[\s\S]*?<\|/\1\|>)");
    static const std::regex tokens(R"(<\|[a-z_/]+\|>)");
    static const std::regex fence(R"(^\s*```[a-z]*\s*\n([\s\S]*?)\n\s*```\s*$)");
    static const std::regex blank_lines(R"(\n[ \t]*\n(?:[ \t]*\n)+)");
    std::string text = std::regex_replace(raw, boxes, "");
    text = std::regex_replace(text, tokens, "");
    std::smatch m;
    if (std::regex_match(text, m, fence))
        text = m[1].str();
    text = std::regex_replace(text, blank_lines, "\n\n");
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

json ocr_request(const OcrModel& model, const std::string& image_data_url) {
    json content = json::array({{{"type", "image_url"}, {"image_url", {{"url", image_data_url}}}},
                                {{"type", "text"}, {"text", model.prompt}}});
    return {{"messages", json::array({{{"role", "user"}, {"content", content}}})},
            {"temperature", 0},
            {"max_tokens", 4096},
            {"stream", false}};
}

std::vector<std::string> ocr_server_args(const OcrModel& model, const std::string& dir, int port) {
    std::vector<std::string> args = {"-m",        (fs::path(dir) / model.model_file).string(),
                                     "--mmproj",  (fs::path(dir) / model.mmproj_file).string(),
                                     "--host",    "127.0.0.1",
                                     "--port",    std::to_string(port),
                                     "-ngl",      "999",
                                     "-np",       "1",
                                     "--no-webui"};
    args.insert(args.end(), model.server_args.begin(), model.server_args.end());
    return args;
}

std::string find_llama_server(const std::string& exe_dir) {
#ifdef _WIN32
    const std::string ext = ".exe";
#else
    const std::string ext;
#endif
    std::vector<fs::path> candidates;
    if (!exe_dir.empty()) {
        const fs::path dir(exe_dir);
        candidates = {dir / ("llama-server" + ext), dir / "bin" / ("llama-server" + ext),
                      dir / ".." / ("llama-server" + ext), dir / ".." / "bin" / ("llama-server" + ext)};
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            // Tauri's sidecar keeps the target triple: llama-server-aarch64-apple-darwin.
            if (entry.path().filename().string().rfind("llama-server-", 0) == 0)
                candidates.push_back(entry.path());
        }
    }
#ifndef _WIN32
    candidates.push_back("/opt/homebrew/bin/llama-server");
    candidates.push_back("/usr/local/bin/llama-server");
    candidates.push_back("/usr/bin/llama-server");
#endif
    for (const auto& c : candidates) {
        std::error_code ec;
        if (fs::is_regular_file(c, ec))
            return c.string();
    }
    return "";
}

namespace {

int free_port() {
#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET)
        return 0;
#else
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return 0;
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    int port = 0;
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
        socklen_t len = sizeof(addr);
        if (getsockname(s, reinterpret_cast<sockaddr*>(&addr), &len) == 0)
            port = ntohs(addr.sin_port);
    }
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
    return port;
}

bool healthy(int port) {
    httplib::Client cli("127.0.0.1", port);
    cli.set_connection_timeout(1, 0);
    cli.set_read_timeout(2, 0);
    auto res = cli.Get("/health");
    return res && res->status == 200;
}

bool file_ready(const fs::path& p) {
    std::error_code ec;
    return fs::is_regular_file(p, ec) && fs::file_size(p, ec) > 0;
}

} // namespace

OcrReader& OcrReader::instance() {
    static OcrReader reader;
    return reader;
}

OcrReader::~OcrReader() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        quit_ = true;
        stop_locked();
    }
    idle_cv_.notify_all();
    if (idle_thread_.joinable())
        idle_thread_.join();
}

void OcrReader::configure(const std::string& root_dir, const std::string& server_bin, int ram_gb, Hooks hooks) {
    std::lock_guard<std::mutex> lock(mutex_);
    root_ = root_dir;
    server_bin_ = server_bin;
    ram_gb_ = ram_gb;
    hooks_ = std::move(hooks);
    model_ = &pick_ocr_model(ram_gb, [this](const OcrModel& m) {
        const fs::path dir = fs::path(root_) / m.id;
        return file_ready(dir / m.model_file) && file_ready(dir / m.mmproj_file);
    });
    // Partial downloads from a run that was stopped mid-transfer.
    std::error_code ec;
    fs::create_directories(root_, ec);
    for (auto it = fs::recursive_directory_iterator(root_, ec);
         !downloading_ && !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        if (it->path().extension() == ".tmp")
            fs::remove(it->path(), ec);
}

const OcrModel& OcrReader::model() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return model_ ? *model_ : ocr_models().front();
}

std::string OcrReader::dir() const {
    return (fs::path(root_) / (model_ ? *model_ : ocr_models().front()).id).string();
}

bool OcrReader::installed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto& m = model_ ? *model_ : ocr_models().front();
    return !root_.empty() && file_ready(fs::path(dir()) / m.model_file) && file_ready(fs::path(dir()) / m.mmproj_file);
}

json OcrReader::status() const {
    const auto& m = model();
    const bool ready = installed();
    const long long done = done_bytes_.load();
    json out = {{"model", {{"id", m.id}, {"display_name", m.display_name}, {"size_bytes", m.size_bytes}}},
                {"installed", ready},
                {"downloading", downloading_.load()},
                {"current_bytes", ready ? m.size_bytes : done},
                {"total_bytes", m.size_bytes},
                {"progress", ready ? 100.0 : (m.size_bytes ? 100.0 * done / m.size_bytes : 0.0)}};
    std::lock_guard<std::mutex> lock(mutex_);
    if (!download_error_.empty())
        out["error"] = download_error_;
    return out;
}

bool OcrReader::start_download() {
    if (installed() || downloading_.exchange(true))
        return false;
    cancelled_ = false;
    done_bytes_ = 0;
    OcrModel m;
    std::string target;
    Hooks hooks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        download_error_.clear();
        m = model_ ? *model_ : ocr_models().front();
        target = dir();
        hooks = hooks_;
    }
    std::thread([this, m, target, hooks] {
        std::error_code ec;
        fs::create_directories(target, ec);
        std::string error;
        long long before = 0;
        for (const auto& file : {m.model_file, m.mmproj_file}) {
            const fs::path dest = fs::path(target) / file;
            std::error_code size_ec;
            if (file_ready(dest)) {
                before += static_cast<long long>(fs::file_size(dest, size_ec));
                done_bytes_ = before;
                continue;
            }
            const std::string url = "https://huggingface.co/" + m.repo + "/resolve/main/" + file;
            const bool ok = !cancelled_ && hooks.download &&
                            hooks.download(url, dest.string(),
                                           [this, before](long long now, long long) { done_bytes_ = before + now; });
            if (!ok) {
                error = cancelled_ ? "Download cancelled" : "Download failed. Check your internet connection.";
                break;
            }
            before += static_cast<long long>(fs::file_size(dest, size_ec));
            done_bytes_ = before;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            download_error_ = error;
        }
        downloading_ = false;
    }).detach();
    return true;
}

void OcrReader::cancel_download() {
    if (!downloading_)
        return;
    cancelled_ = true;
    Hooks hooks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        hooks = hooks_;
    }
    if (hooks.cancel)
        hooks.cancel();
}

void OcrReader::use_server_for_tests(int port) {
    std::lock_guard<std::mutex> lock(mutex_);
    port_ = port;
    test_server_ = true;
}

void OcrReader::set_idle_seconds(int seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    idle_seconds_ = seconds;
}

bool OcrReader::ensure_server(std::string& error) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (test_server_)
        return true;
    if (pid_ != 0 && healthy(port_))
        return true;
    stop_locked();
    if (server_bin_.empty()) {
        error = "The text reader needs llama-server, which was not found next to Delta.";
        return false;
    }
    const int port = free_port();
    if (port == 0) {
        error = "No free port for the text reader.";
        return false;
    }
    const auto args = ocr_server_args(*model_, dir(), port);
    const std::string log_path = (fs::path(dir()) / "server.log").string();

#ifdef _WIN32
    std::string cmd = "\"" + server_bin_ + "\"";
    for (const auto& a : args)
        cmd += " \"" + a + "\"";
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    HANDLE log =
        CreateFileA(log_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    if (log != INVALID_HANDLE_VALUE) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = log;
        si.hStdError = log;
    }
    std::vector<char> line(cmd.begin(), cmd.end());
    line.push_back('\0');
    const BOOL ok = CreateProcessA(NULL, line.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (log != INVALID_HANDLE_VALUE)
        CloseHandle(log);
    if (!ok) {
        error = "Could not start the text reader.";
        return false;
    }
    CloseHandle(pi.hThread);
    process_ = pi.hProcess;
    pid_ = pi.dwProcessId;
#else
    std::vector<std::string> argv_store = {server_bin_};
    argv_store.insert(argv_store.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (auto& a : argv_store)
        argv.push_back(a.data());
    argv.push_back(nullptr);
    const pid_t pid = fork();
    if (pid == 0) {
        setsid();
        const int fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        close(STDIN_FILENO);
        execv(argv[0], argv.data());
        _exit(127);
    }
    if (pid < 0) {
        error = "Could not start the text reader.";
        return false;
    }
    pid_ = pid;
#endif
    port_ = port;
    last_used_ = std::chrono::steady_clock::now();
    if (!idle_thread_.joinable())
        idle_thread_ = std::thread([this] { watch_idle(); });

    // Loading the model takes a few seconds; give a cold disk up to three minutes.
    lock.unlock();
    for (int i = 0; i < 360; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        {
            std::lock_guard<std::mutex> guard(mutex_);
            if (pid_ == 0)
                break;
#ifdef _WIN32
            if (WaitForSingleObject(static_cast<HANDLE>(process_), 0) == WAIT_OBJECT_0)
                break;
#else
            int st = 0;
            if (waitpid(static_cast<pid_t>(pid_), &st, WNOHANG) != 0) {
                pid_ = 0;
                break;
            }
#endif
        }
        if (healthy(port))
            return true;
    }
    lock.lock();
    stop_locked();
    error = "The text reader could not start. See " + log_path;
    return false;
}

void OcrReader::stop_locked() {
    if (pid_ == 0)
        return;
#ifdef _WIN32
    if (process_) {
        TerminateProcess(static_cast<HANDLE>(process_), 0);
        WaitForSingleObject(static_cast<HANDLE>(process_), 3000);
        CloseHandle(static_cast<HANDLE>(process_));
        process_ = nullptr;
    }
#else
    const pid_t pid = static_cast<pid_t>(pid_);
    kill(pid, SIGTERM);
    int st = 0;
    bool gone = false;
    for (int i = 0; i < 30 && !gone; ++i) {
        if (waitpid(pid, &st, WNOHANG) != 0)
            gone = true;
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!gone) {
        kill(pid, SIGKILL);
        waitpid(pid, &st, 0);
    }
#endif
    pid_ = 0;
}

void OcrReader::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_locked();
}

void OcrReader::watch_idle() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!quit_) {
        idle_cv_.wait_for(lock, std::chrono::seconds(1));
        if (quit_ || pid_ == 0)
            continue;
        if (std::chrono::steady_clock::now() - last_used_ < std::chrono::seconds(idle_seconds_))
            continue;
        // A page being read keeps the reader running.
        lock.unlock();
        std::unique_lock<std::mutex> reading(read_mutex_, std::try_to_lock);
        lock.lock();
        if (reading.owns_lock() && pid_ != 0 &&
            std::chrono::steady_clock::now() - last_used_ >= std::chrono::seconds(idle_seconds_))
            stop_locked();
    }
}

OcrReader::Page OcrReader::read_page(const std::string& image_data_url) {
    std::lock_guard<std::mutex> reading(read_mutex_);
    Page page;
    bool test_server = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        test_server = test_server_;
    }
    if (!test_server && !installed()) {
        page.status = 409;
        page.error = "The text reader is not downloaded yet.";
        return page;
    }
    if (!ensure_server(page.error)) {
        page.status = 503;
        return page;
    }
    int port = 0;
    json body;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        port = port_;
        body = ocr_request(model_ ? *model_ : ocr_models().front(), image_data_url);
    }
    httplib::Client cli("127.0.0.1", port);
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(300, 0); // a dense page on CPU
    auto res = cli.Post("/v1/chat/completions", body.dump(), "application/json");
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_used_ = std::chrono::steady_clock::now();
    }
    if (!res || res->status != 200) {
        page.status = 503;
        page.error = "The text reader did not answer.";
        return page;
    }
    try {
        const json reply = json::parse(res->body);
        page.text = clean_ocr_text(reply.at("choices").at(0).at("message").value("content", ""));
    } catch (const std::exception&) {
        page.status = 503;
        page.error = "The text reader gave an unreadable answer.";
    }
    return page;
}

} // namespace agent
} // namespace delta
