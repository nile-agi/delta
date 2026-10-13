#include "openai_api.h"

#include <cpp-httplib/httplib.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using json = nlohmann::json;

namespace delta {
namespace api {

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

bool starts_with(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

// Compares every byte so a wrong key takes as long to reject as a nearly right one.
bool same_key(const std::string& a, const std::string& b) {
    unsigned char diff = a.size() == b.size() ? 0 : 1;
    for (size_t i = 0; i < a.size(); i++)
        diff |= (unsigned char)(a[i] ^ (i < b.size() ? b[i] : 0));
    return diff == 0;
}

} // namespace

// ---- Access settings ------------------------------------------------------------------------

void ApiAccess::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (path_.empty())
        return;
    try {
        std::ifstream in(path_);
        if (!in)
            return;
        json j = json::parse(in);
        saved_.network = j.value("network", false);
        saved_.api_key = j.value("api_key", std::string());
        // A shared address with no key would let anyone on the network in.
        if (saved_.api_key.empty())
            saved_.network = false;
    } catch (...) {
        saved_ = ApiAccessSettings{};
    }
}

bool ApiAccess::save(const ApiAccessSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!path_.empty()) {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::create_directories(fs::path(path_).parent_path(), ec);
        {
            std::ofstream out(path_, std::ios::trunc);
            if (!out)
                return false;
            out << json{{"network", settings.network}, {"api_key", settings.api_key}}.dump(2) << "\n";
            if (!out)
                return false;
        }
        fs::permissions(path_, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ec);
    }
    saved_ = settings;
    return true;
}

void ApiAccess::set_overrides(const std::string& host, const std::string& api_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    host_override_ = host;
    key_override_ = api_key;
}

ApiAccessSettings ApiAccess::saved() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return saved_;
}

std::string ApiAccess::host() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!host_override_.empty())
        return host_override_;
    return saved_.network && !saved_.api_key.empty() ? "0.0.0.0" : "127.0.0.1";
}

std::string ApiAccess::api_key() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return key_override_.empty() ? saved_.api_key : key_override_;
}

bool ApiAccess::overridden() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !host_override_.empty() || !key_override_.empty();
}

// ---- Request gate ---------------------------------------------------------------------------

bool is_loopback(const std::string& address) {
    const std::string a = lower(address);
    return starts_with(a, "127.") || a == "::1" || a == "localhost" || starts_with(a, "::ffff:127.");
}

bool is_openai_path(const std::string& path) {
    return path == "/v1/chat/completions" || path == "/v1/completions" || path == "/v1/embeddings" ||
           path == "/v1/models" || starts_with(path, "/v1/models/");
}

bool trusted_origin(const std::string& origin) {
    if (origin.empty())
        return true;
    const std::string o = lower(origin);
    if (starts_with(o, "tauri://"))
        return true;
    const auto scheme_end = o.find("://");
    if (scheme_end == std::string::npos)
        return false;
    std::string host = o.substr(scheme_end + 3);
    host = host.substr(0, host.find('/'));
    if (starts_with(host, "[")) {
        host = host.substr(0, host.find(']') + 1);
    } else {
        host = host.substr(0, host.find(':'));
    }
    return host == "localhost" || host == "127.0.0.1" || host == "[::1]" || host == "tauri.localhost";
}

Gate check_request(const std::string& method, const std::string& path, const std::string& remote_addr,
                   const std::string& authorization, const std::string& x_api_key, const std::string& api_key) {
    if (method == "OPTIONS")
        return {};
    const bool openai = is_openai_path(path);
    if (!is_loopback(remote_addr) && !openai)
        return {403, openai_error("Only the OpenAI-compatible API (/v1) is available from other devices.",
                                  "permission_error", "forbidden")};
    if (api_key.empty() || !openai)
        return {};

    std::string given = x_api_key;
    if (lower(authorization.substr(0, 7)) == "bearer ") {
        given = authorization.substr(7);
        given.erase(0, given.find_first_not_of(' '));
    }
    if (same_key(given, api_key))
        return {};
    return {401, openai_error(given.empty() ? "Missing API key. Send it as 'Authorization: Bearer <key>'."
                                            : "Incorrect API key provided.",
                              "invalid_request_error", "invalid_api_key")};
}

bool is_delta_request(const json& body) {
    if (!body.is_object())
        return false;
    for (const char* key : {"use_tools", "conversation_id", "use_documents", "task_id"}) {
        if (body.contains(key))
            return true;
    }
    for (auto it = body.begin(); it != body.end(); ++it) {
        const std::string& k = it.key();
        if (starts_with(k, "use_") && k.size() > 10 && k.compare(k.size() - 6, 6, "_tools") == 0)
            return true;
    }
    return false;
}

json openai_error(const std::string& message, const std::string& type, const std::string& code) {
    json err = {{"message", message}, {"type", type}, {"param", nullptr}, {"code", nullptr}};
    if (!code.empty())
        err["code"] = code;
    return json{{"error", err}};
}

// ---- Proxy ----------------------------------------------------------------------------------

namespace {

constexpr time_t kReadTimeoutSec = 600; // a slow CPU model can think for minutes

json upstream_error(int status, const std::string& body) {
    try {
        json j = json::parse(body);
        if (j.is_object() && j.contains("error"))
            return j;
    } catch (...) {
    }
    if (status == 0)
        return openai_error("The model server did not answer. Is a model loaded?", "server_error", "model_unavailable");
    return openai_error(body.empty() ? "The model server returned HTTP " + std::to_string(status) : body,
                        "server_error");
}

// Renames the model in one JSON reply or one `data: {...}` event line; anything else passes as is.
std::string rename_model(const std::string& text, const std::string& model) {
    const bool event = text.rfind("data: {", 0) == 0;
    if (model.empty() || (!event && (text.empty() || text[0] != '{')))
        return text;
    try {
        json j = json::parse(event ? text.substr(6) : text);
        if (!j.is_object() || !j.contains("model"))
            return text;
        j["model"] = model;
        return event ? "data: " + j.dump() : j.dump();
    } catch (...) {
        return text;
    }
}

} // namespace

void proxy_openai(httplib::Response& res, const std::string& path, const json& body, const std::string& host, int port,
                  const std::string& reply_model) {
    const std::string payload = body.dump();
    if (!body.value("stream", false)) {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(5, 0);
        cli.set_read_timeout(kReadTimeoutSec, 0);
        auto up = cli.Post(path, payload, "application/json");
        if (!up) {
            res.status = 502;
            res.set_content(upstream_error(0, "").dump(), "application/json");
            return;
        }
        res.status = up->status;
        if (up->status >= 400) {
            res.set_content(upstream_error(up->status, up->body).dump(), "application/json");
            return;
        }
        const std::string type = up->get_header_value("Content-Type");
        res.set_content(rename_model(up->body, reply_model), type.empty() ? "application/json" : type);
        return;
    }

    struct Job {
        std::string path, payload, host, reply_model;
        int port = 0;
        bool started = false;
    };
    auto job = std::make_shared<Job>();
    job->path = path;
    job->payload = payload;
    job->host = host;
    job->port = port;
    job->reply_model = reply_model;

    res.set_header("Cache-Control", "no-cache");
    res.set_header("X-Accel-Buffering", "no");
    res.set_chunked_content_provider("text/event-stream", [job](size_t, httplib::DataSink& sink) -> bool {
        if (job->started) {
            sink.done();
            return true;
        }
        job->started = true;

        httplib::Client cli(job->host, job->port);
        cli.set_connection_timeout(5, 0);
        cli.set_read_timeout(kReadTimeoutSec, 0);
        // An error arrives as one JSON object rather than events; it is held back and resent as one.
        bool first = true, plain = false;
        std::string held, partial;
        // Events are renamed whole lines at a time, since a chunk can end mid-line.
        auto forward = [&](const char* data, size_t len) -> bool {
            if (job->reply_model.empty())
                return sink.write(data, len);
            partial.append(data, len);
            std::string out;
            size_t start = 0, nl;
            while ((nl = partial.find('\n', start)) != std::string::npos) {
                out += rename_model(partial.substr(start, nl - start), job->reply_model) + "\n";
                start = nl + 1;
            }
            partial.erase(0, start);
            return out.empty() || sink.write(out.data(), out.size());
        };
        auto up = cli.Post(job->path, httplib::Headers{{"Accept", "text/event-stream"}}, job->payload,
                           "application/json", [&](const char* data, size_t len) {
                               if (first) {
                                   first = false;
                                   size_t i = 0;
                                   while (i < len && std::isspace((unsigned char)data[i]))
                                       i++;
                                   plain = i < len && data[i] == '{';
                               }
                               if (plain) {
                                   held.append(data, len);
                                   return true;
                               }
                               return forward(data, len);
                           });
        if (!partial.empty())
            sink.write(partial.data(), partial.size());
        const int status = up ? up->status : 0;
        if (plain || status == 0 || (status >= 400 && first)) {
            const std::string frame = "data: " + upstream_error(status, held).dump() + "\n\n";
            sink.write(frame.data(), frame.size());
        }
        sink.done();
        return true;
    });
}

// ---- Keys and addresses ---------------------------------------------------------------------

std::string generate_api_key() {
    std::random_device rd;
    static const char* hex = "0123456789abcdef";
    std::string key = "delta-";
    for (int i = 0; i < 40; i++)
        key += hex[rd() & 0xF];
    return key;
}

std::string mask_api_key(const std::string& key) {
    if (key.size() <= 12)
        return key.empty() ? "" : "••••";
    return key.substr(0, 8) + "…" + key.substr(key.size() - 4);
}

std::string lan_address() {
    // Connecting a UDP socket sends nothing; it only makes the OS pick the outgoing interface.
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == INVALID_SOCKET)
        return "";
#else
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return "";
#endif
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
    std::string out;
    if (connect(fd, (sockaddr*)&remote, sizeof(remote)) == 0) {
        sockaddr_in local{};
        socklen_t len = sizeof(local);
        char buf[INET_ADDRSTRLEN] = {0};
        if (getsockname(fd, (sockaddr*)&local, &len) == 0 && inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf)))
            out = buf;
    }
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
    if (out == "0.0.0.0" || is_loopback(out))
        return "";
    return out;
}

// ---- Developer routes -----------------------------------------------------------------------

namespace {

json server_state(const DeveloperRoutes& routes, bool reveal_key) {
    const int port = routes.port ? routes.port() : 0;
    const std::string host = routes.access->host();
    const std::string key = routes.access->api_key();
    json network_url = nullptr;
    if (!is_loopback(host)) {
        const std::string address = host == "0.0.0.0" ? lan_address() : host;
        if (!address.empty())
            network_url = "http://" + address + ":" + std::to_string(port) + "/v1";
    }
    return json{{"network", !is_loopback(host)},
                {"host", host},
                {"port", port},
                {"local_url", "http://127.0.0.1:" + std::to_string(port) + "/v1"},
                {"network_url", network_url},
                {"has_key", !key.empty()},
                {"api_key", reveal_key ? key : mask_api_key(key)},
                {"overridden", routes.access->overridden()}};
}

void send_json(httplib::Response& res, int status, const json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

} // namespace

void register_developer_routes(httplib::Server& server, DeveloperRoutes routes) {
    server.Get("/api/developer/server", [routes](const httplib::Request& req, httplib::Response& res) {
        // The key is only shown to Delta's own pages; any other site gets the masked form.
        send_json(res, 200, server_state(routes, trusted_origin(req.get_header_value("Origin"))));
    });

    server.Put("/api/developer/server", [routes](const httplib::Request& req, httplib::Response& res) {
        if (!trusted_origin(req.get_header_value("Origin"))) {
            send_json(res, 403, openai_error("Only Delta can change these settings.", "permission_error"));
            return;
        }
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            send_json(res, 400, openai_error("Invalid JSON in request body", "invalid_request_error"));
            return;
        }
        ApiAccessSettings s = routes.access->saved();
        const std::string old_host = routes.access->host();
        if (body.contains("network") && body["network"].is_boolean())
            s.network = body["network"].get<bool>();
        const std::string key = body.value("key", std::string());
        if (key == "generate") {
            s.api_key = generate_api_key();
        } else if (key == "remove") {
            if (s.network) {
                send_json(res, 400,
                          openai_error("Turn off network access before removing the key.", "invalid_request_error"));
                return;
            }
            s.api_key.clear();
        }
        if (s.network && s.api_key.empty())
            s.api_key = generate_api_key();
        if (!routes.access->save(s)) {
            send_json(res, 500, openai_error("Could not save the API settings.", "server_error"));
            return;
        }
        if (routes.access->host() != old_host && routes.rebind)
            routes.rebind();
        send_json(res, 200, server_state(routes, true));
    });
}

} // namespace api
} // namespace delta
