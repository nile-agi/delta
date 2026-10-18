#pragma once
#include "runtime_config.h"
#include "runtime_failure.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <cstdint>

namespace delta {
enum class EngineState { Stopped, Starting, Available, Failed };
enum class ModelLoadState { Loading, Ready, Failed, Cancelled, TimedOut };
const char* to_string(EngineState);
const char* to_string(ModelLoadState);
struct RuntimeSnapshot {
    std::string attempt_id;
    uint64_t generation = 0;
    EngineState engine_state = EngineState::Starting;
    ModelLoadState state = ModelLoadState::Loading;
    RuntimeConfig config;
    std::optional<RuntimeFailure> failure;
};
struct WorkerProbeResult {
    bool ready = false;
    bool failed = false;
    bool engine_available = true;
    RuntimeFailure failure;
};
class ModelRuntime {
  public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;
    using Cancelled = std::function<bool()>;
    struct Hooks {
        std::function<void(const RuntimeConfig&, const Cancelled&)> launch;
        std::function<WorkerProbeResult(const RuntimeConfig&)> probe;
        std::function<void()> stop;
        std::function<RuntimeFailure(const RuntimeFailure&)> classify;
    };
    explicit ModelRuntime(Hooks, Clock clock = std::chrono::steady_clock::now);
    ~ModelRuntime();
    RuntimeSnapshot select(const RuntimeConfig&);
    std::optional<RuntimeSnapshot> current() const;
    std::optional<RuntimeSnapshot> get(const std::string&) const;
    std::optional<RuntimeSnapshot> cancel(const std::string&);
    void shutdown();

  private:
    void run();
    bool cancelled(uint64_t) const;
    void finish(uint64_t, ModelLoadState, std::optional<RuntimeFailure> = std::nullopt);
    Hooks hooks_;
    Clock clock_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<RuntimeSnapshot> attempts_;
    uint64_t generation_ = 0;
    bool stopping_ = false;
    std::chrono::steady_clock::time_point deadline_;
    std::thread worker_;
};
} // namespace delta
