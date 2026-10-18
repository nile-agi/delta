#include "model_runtime.h"
#include <stdexcept>
namespace delta {
const char* to_string(EngineState s) {
    switch (s) {
    case EngineState::Stopped:
        return "stopped";
    case EngineState::Starting:
        return "starting";
    case EngineState::Available:
        return "available";
    case EngineState::Failed:
        return "failed";
    }
    return "failed";
}
const char* to_string(ModelLoadState s) {
    switch (s) {
    case ModelLoadState::Loading:
        return "loading";
    case ModelLoadState::Ready:
        return "ready";
    case ModelLoadState::Failed:
        return "failed";
    case ModelLoadState::Cancelled:
        return "cancelled";
    case ModelLoadState::TimedOut:
        return "timed_out";
    }
    return "failed";
}
ModelRuntime::ModelRuntime(Hooks hooks, Clock clock) : hooks_(std::move(hooks)), clock_(std::move(clock)) {
    worker_ = std::thread([this] { run(); });
}
ModelRuntime::~ModelRuntime() {
    shutdown();
}
RuntimeSnapshot ModelRuntime::select(const RuntimeConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_)
        throw std::runtime_error("Engine is stopping");
    if (!attempts_.empty() && attempts_.back().state == ModelLoadState::Loading)
        attempts_.back().state = ModelLoadState::Cancelled;
    RuntimeSnapshot s;
    s.generation = ++generation_;
    s.attempt_id =
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(generation_);
    s.config = config;
    s.config.generation = s.generation;
    attempts_.push_back(s);
    while (attempts_.size() > 16)
        attempts_.pop_front();
    deadline_ = clock_() + std::chrono::seconds(180);
    wake_.notify_all();
    return s;
}
std::optional<RuntimeSnapshot> ModelRuntime::current() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (attempts_.empty())
        return std::nullopt;
    return attempts_.back();
}
std::optional<RuntimeSnapshot> ModelRuntime::get(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& s : attempts_)
        if (s.attempt_id == id)
            return s;
    return std::nullopt;
}
std::optional<RuntimeSnapshot> ModelRuntime::cancel(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : attempts_)
        if (s.attempt_id == id) {
            if (s.state == ModelLoadState::Loading || s.state == ModelLoadState::Ready)
                s.state = ModelLoadState::Cancelled;
            wake_.notify_all();
            return s;
        }
    return std::nullopt;
}
bool ModelRuntime::cancelled(uint64_t generation) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stopping_ || attempts_.empty() || attempts_.back().generation != generation ||
           (attempts_.back().state != ModelLoadState::Loading && attempts_.back().state != ModelLoadState::Ready) ||
           (attempts_.back().state == ModelLoadState::Loading && clock_() >= deadline_);
}
void ModelRuntime::finish(uint64_t gen, ModelLoadState state, std::optional<RuntimeFailure> failure) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_ || attempts_.empty() || attempts_.back().generation != gen ||
        (attempts_.back().state != ModelLoadState::Loading && attempts_.back().state != ModelLoadState::Ready))
        return;
    auto& s = attempts_.back();
    if (s.state == ModelLoadState::Loading && clock_() >= deadline_) {
        state = ModelLoadState::TimedOut;
        failure = RuntimeFailure{"timeout", "Model took too long to load. Retry or choose a smaller model."};
    }
    s.state = state;
    s.engine_state = state == ModelLoadState::Ready ? EngineState::Available : EngineState::Stopped;
    s.failure = std::move(failure);
}
void ModelRuntime::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        wake_.notify_all();
    }
    if (worker_.joinable())
        worker_.join();
}
void ModelRuntime::run() {
    uint64_t handled = 0;
    auto stop = [&] {
        try {
            hooks_.stop();
        } catch (...) {
        }
    };
    while (true) {
        RuntimeSnapshot s;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock,
                       [&] { return stopping_ || (!attempts_.empty() && attempts_.back().generation != handled); });
            if (stopping_)
                break;
            s = attempts_.back();
            handled = s.generation;
        }
        stop();
        try {
            auto interrupted = [this, gen = s.generation] { return cancelled(gen); };
            if (!interrupted())
                hooks_.launch(s.config, interrupted);
            while (!interrupted()) {
                const auto p = hooks_.probe(s.config);
                if (p.failed) {
                    finish(s.generation, ModelLoadState::Failed, p.failure);
                    break;
                }
                if (p.ready) {
                    finish(s.generation, ModelLoadState::Ready);
                }
                std::unique_lock<std::mutex> lock(mutex_);
                if (!attempts_.empty() && attempts_.back().generation == s.generation)
                    attempts_.back().engine_state = p.engine_available ? EngineState::Available : EngineState::Starting;
                wake_.wait_for(lock, std::chrono::milliseconds(p.ready ? 1000 : 250));
            }
            finish(s.generation, ModelLoadState::TimedOut);
        } catch (const std::exception& e) {
            finish(s.generation, ModelLoadState::Failed, RuntimeFailure{"runtime", e.what()});
        } catch (...) {
            finish(s.generation, ModelLoadState::Failed, RuntimeFailure{"runtime", "Unexpected model load failure"});
        }
        auto current_state = current();
        if (!current_state || current_state->generation != s.generation ||
            current_state->state != ModelLoadState::Ready)
            stop();
    }
    stop();
}
} // namespace delta
