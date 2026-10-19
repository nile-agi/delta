#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace delta {
struct ProcessSpec {
    std::vector<std::string> args;
    std::string cwd;
    uint64_t generation = 0;
};
struct ProcessExit {
    int code = 0;
    int signal = 0;
};
class OwnedProcess {
  public:
    // start/alive/stop are serialized by the owner. Output callbacks run on the reader;
    // they must not destroy this object or call stop(). stop() drains and joins them.
    using OutputCallback = std::function<void(uint64_t, const std::string&)>;
    using ExitCallback = std::function<void(uint64_t, ProcessExit)>;
    ~OwnedProcess();
    void start(const ProcessSpec&, OutputCallback, ExitCallback);
    void stop();
    bool alive();
    ProcessExit exit_status() const { return exit_; }

  private:
    void read_output();
    ProcessSpec spec_;
    OutputCallback output_;
    ExitCallback on_exit_;
    ProcessExit exit_;
    bool reaped_ = true;
    std::atomic<bool> drain_{false};
    std::thread reader_;
#ifdef _WIN32
    HANDLE process_ = nullptr, job_ = nullptr, read_pipe_ = nullptr;
    DWORD pid_ = 0;
#else
    int pid_ = 0, read_pipe_ = -1;
#endif
};
} // namespace delta
