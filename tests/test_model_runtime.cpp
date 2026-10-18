#include "model_runtime.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <stdexcept>
using namespace delta;
using namespace std::chrono_literals;
void require(bool ok, const char* text) {
    if (!ok)
        throw std::runtime_error(text);
}
template <class F> void eventually(F fn) {
    for (int i = 0; i < 400; ++i) {
        if (fn())
            return;
        std::this_thread::sleep_for(5ms);
    }
    throw std::runtime_error("state transition did not finish");
}
int main() {
    try {
        std::atomic<int> probe_state{0}, starts{0}, stops{0};
        std::atomic<long long> seconds{0};
        ModelRuntime::Hooks hooks;
        hooks.launch = [&](const RuntimeConfig&, const ModelRuntime::Cancelled&) { ++starts; };
        hooks.probe = [&](const RuntimeConfig&) {
            WorkerProbeResult r;
            r.ready = probe_state == 1;
            r.failed = probe_state == 2;
            r.failure = {"worker_exit", "Worker exited"};
            return r;
        };
        hooks.stop = [&] { ++stops; };
        auto epoch = std::chrono::steady_clock::now();
        ModelRuntime runtime(hooks, [&] { return epoch + std::chrono::seconds(seconds.load()); });
        RuntimeConfig config;
        config.model_name = "test";
        config.model_alias = "test";
        config.context = 2048;
        auto first = runtime.select(config);
        require(first.state == ModelLoadState::Loading, "acceptance must start loading");
        eventually([&] { return starts > 0; });
        require(runtime.current()->state == ModelLoadState::Loading, "healthy router is not worker ready");
        auto second = runtime.select(config);
        require(second.attempt_id != first.attempt_id, "same-model retry must have a fresh ID");
        require(runtime.get(first.attempt_id)->state == ModelLoadState::Cancelled, "superseded attempt is cancelled");
        runtime.cancel(first.attempt_id);
        require(runtime.current()->state == ModelLoadState::Loading, "old cancel cannot cancel current");
        probe_state = 1;
        eventually([&] { return runtime.current()->state == ModelLoadState::Ready; });
        require(runtime.current()->attempt_id == second.attempt_id, "stale completion cannot replace newer attempt");
        probe_state = 2;
        eventually([&] { return runtime.current()->state == ModelLoadState::Failed; });
        require(runtime.current()->failure->code == "worker_exit", "post-ready child exit must be observable");
        probe_state = 0;
        auto third = runtime.select(config);
        eventually([&] { return starts >= 3; });
        seconds = 180;
        eventually([&] { return runtime.current()->state == ModelLoadState::TimedOut; });
        probe_state = 1;
        std::this_thread::sleep_for(60ms);
        require(runtime.current()->state == ModelLoadState::TimedOut, "late success cannot undo timeout");
        runtime.cancel(third.attempt_id);
        require(runtime.current()->state == ModelLoadState::TimedOut, "terminal cancellation is idempotent");
        for (int i = 0; i < 17; ++i)
            runtime.select(config);
        require(!runtime.get(first.attempt_id), "attempt retention must be bounded");
        runtime.cancel(runtime.current()->attempt_id);
        require(runtime.current()->state == ModelLoadState::Cancelled, "current load can be cancelled");
        runtime.shutdown();
        require(stops > 0, "shutdown must clean the owned worker");
        std::cout << "Runtime attempt, stale result, deadline, retention and cleanup checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
