#include "owned_process.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace delta {
using namespace std::chrono_literals;
OwnedProcess::~OwnedProcess() {
    stop();
}
#ifdef _WIN32
static std::wstring wide(const std::string& value) {
    if (value.empty())
        return {};
    int size =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (!size)
        throw std::runtime_error("Invalid UTF-8 process argument");
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(),
                        size);
    return result;
}
static std::wstring quote(const std::string& value) {
    std::wstring result = L"\"";
    int slashes = 0;
    for (auto ch : wide(value)) {
        if (ch == L'\\') {
            ++slashes;
            continue;
        }
        result.append(ch == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        result += ch;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}
#endif
void OwnedProcess::start(const ProcessSpec& spec, OutputCallback output, ExitCallback on_exit) {
    stop();
    if (spec.args.empty())
        throw std::invalid_argument("Missing executable");
    spec_ = spec;
    output_ = std::move(output);
    on_exit_ = std::move(on_exit);
    exit_ = {};
    drain_ = false;
#ifdef _WIN32
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE write_pipe = nullptr;
    if (!CreatePipe(&read_pipe_, &write_pipe, &security, 0))
        throw std::runtime_error("Cannot capture process output");
    SetHandleInformation(read_pipe_, HANDLE_FLAG_INHERIT, 0);
    job_ = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job_ || !SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        CloseHandle(write_pipe);
        stop();
        throw std::runtime_error("Cannot create owned process job");
    }
    std::wstring command;
    for (const auto& arg : spec.args) {
        if (!command.empty())
            command += L' ';
        command += quote(arg);
    }
    auto cwd = wide(spec.cwd);
    STARTUPINFOEXW info{};
    info.StartupInfo.cb = sizeof(info);
    info.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    info.StartupInfo.hStdOutput = write_pipe;
    info.StartupInfo.hStdError = write_pipe;
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<unsigned char> attributes(bytes);
    info.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (!InitializeProcThreadAttributeList(info.lpAttributeList, 1, 0, &bytes)) {
        CloseHandle(write_pipe);
        stop();
        throw std::runtime_error("Cannot initialize child handles");
    }
    if (!UpdateProcThreadAttribute(info.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &write_pipe,
                                   sizeof(write_pipe), nullptr, nullptr)) {
        DeleteProcThreadAttributeList(info.lpAttributeList);
        CloseHandle(write_pipe);
        stop();
        throw std::runtime_error("Cannot isolate child handles");
    }
    PROCESS_INFORMATION child{};
    BOOL created = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr,
                                  cwd.empty() ? nullptr : cwd.c_str(), &info.StartupInfo, &child);
    DeleteProcThreadAttributeList(info.lpAttributeList);
    CloseHandle(write_pipe);
    if (!created) {
        stop();
        throw std::runtime_error("Cannot create model process");
    }
    process_ = child.hProcess;
    pid_ = child.dwProcessId;
    if (!AssignProcessToJobObject(job_, process_)) {
        TerminateProcess(process_, 1);
        CloseHandle(child.hThread);
        stop();
        throw std::runtime_error("Cannot own model process job");
    }
    reaped_ = false;
    ResumeThread(child.hThread);
    CloseHandle(child.hThread);
#else
    int pipes[2];
    if (pipe(pipes) != 0)
        throw std::runtime_error("Cannot capture process output");
    fcntl(pipes[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipes[1], F_SETFD, FD_CLOEXEC);
    std::vector<char*> argv;
    for (const auto& arg : spec_.args)
        argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    const pid_t child = fork();
    if (child == 0) {
        close(pipes[0]);
        if (setsid() < 0)
            _exit(126);
        dup2(pipes[1], STDOUT_FILENO);
        dup2(pipes[1], STDERR_FILENO);
        close(pipes[1]);
        if (!spec_.cwd.empty() && chdir(spec_.cwd.c_str()) != 0)
            _exit(126);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(pipes[1]);
    if (child < 0) {
        close(pipes[0]);
        throw std::runtime_error("Cannot create model process");
    }
    pid_ = child;
    read_pipe_ = pipes[0];
    reaped_ = false;
    fcntl(read_pipe_, F_SETFL, fcntl(read_pipe_, F_GETFL) | O_NONBLOCK);
#endif
    try {
        reader_ = std::thread([this] { read_output(); });
    } catch (...) {
        stop();
        throw;
    }
}
bool OwnedProcess::alive() {
    if (reaped_)
        return false;
#ifdef _WIN32
    if (WaitForSingleObject(process_, 0) == WAIT_TIMEOUT)
        return true;
    DWORD code = 0;
    GetExitCodeProcess(process_, &code);
    exit_.code = static_cast<int>(code);
#else
    // Keep the zombie until stop(): its reserved PID protects the owned process group
    // from PID reuse between observing an exit and terminating remaining descendants.
    siginfo_t status{};
    if (waitid(P_PID, pid_, &status, WEXITED | WNOHANG | WNOWAIT) < 0) {
        if (errno == EINTR)
            return true;
        reaped_ = true;
        return false;
    }
    if (status.si_pid == 0)
        return true;
    if (status.si_code == CLD_EXITED)
        exit_.code = status.si_status;
    else {
        exit_.signal = status.si_status;
        exit_.code = 128 + exit_.signal;
    }
    return false;
#endif
    reaped_ = true;
    return false;
}
void OwnedProcess::stop() {
    const bool had_child =
#ifdef _WIN32
        process_ != nullptr;
#else
        pid_ != 0;
#endif
    if (had_child) {
#ifdef _WIN32
        // Console-free children have no portable graceful signal. Give an already-exiting
        // process a bounded opportunity to finish, then terminate only its owned job.
        if (alive())
            WaitForSingleObject(process_, 2000);
        if (job_)
            TerminateJobObject(job_, 0);
#else
        kill(-pid_, SIGTERM);
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        while (alive() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(10ms);
        kill(-pid_, SIGKILL);
        if (!reaped_) {
            int status = 0;
            while (waitpid(pid_, &status, 0) < 0 && errno == EINTR) {
            }
            if (WIFSIGNALED(status)) {
                exit_.signal = WTERMSIG(status);
                exit_.code = 128 + exit_.signal;
            } else if (WIFEXITED(status))
                exit_.code = WEXITSTATUS(status);
            reaped_ = true;
        }
#endif
        alive();
    }
    drain_ = true;
    if (reader_.joinable())
        reader_.join();
#ifdef _WIN32
    if (read_pipe_)
        CloseHandle(read_pipe_);
    if (process_)
        CloseHandle(process_);
    if (job_)
        CloseHandle(job_);
    read_pipe_ = process_ = job_ = nullptr;
    pid_ = 0;
#else
    if (read_pipe_ >= 0)
        close(read_pipe_);
    read_pipe_ = -1;
    pid_ = 0;
#endif
    if (had_child && on_exit_) {
        try {
            on_exit_(spec_.generation, exit_);
        } catch (...) {
        }
    }
}
void OwnedProcess::read_output() {
    std::string line;
    auto emit = [&] {
        if (!line.empty() && output_) {
            try {
                output_(spec_.generation, line);
            } catch (...) {
            }
        }
        line.clear();
    };
    auto drain_deadline = std::chrono::steady_clock::time_point::max();
    while (true) {
        if (drain_ && drain_deadline == std::chrono::steady_clock::time_point::max())
            drain_deadline = std::chrono::steady_clock::now() + 300ms;
        if (std::chrono::steady_clock::now() >= drain_deadline)
            break;
        char buffer[4096];
        int count = 0;
#ifdef _WIN32
        DWORD available = 0, received = 0;
        if (!PeekNamedPipe(read_pipe_, nullptr, 0, nullptr, &available, nullptr))
            break;
        if (available && ReadFile(read_pipe_, buffer, std::min<DWORD>(available, sizeof(buffer)), &received, nullptr))
            count = static_cast<int>(received);
#else
        pollfd fd{read_pipe_, POLLIN, 0};
        poll(&fd, 1, 20);
        count = static_cast<int>(read(read_pipe_, buffer, sizeof(buffer)));
        if (count == 0)
            break;
        if (count < 0 && errno != EAGAIN && errno != EINTR)
            break;
#endif
        if (count <= 0) {
            if (drain_)
                break;
            std::this_thread::sleep_for(10ms);
            continue;
        }
        for (int i = 0; i < count; ++i) {
            if (buffer[i] == '\n' || buffer[i] == '\r')
                emit();
            else if (line.size() < 4096)
                line += buffer[i];
        }
    }
    emit();
}
} // namespace delta
