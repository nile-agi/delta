#include "delta_cli.h"
#include <iostream>

// Run with an isolated HOME: ModelManager persists explicit context overrides there.
int main() {
    int failures = 0;
    auto check = [&](bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    };
    {
        delta::ModelManager manager;
        check(manager.get_max_context_for_model("MiniCPM5:1b") == 8192,
              "a fresh MiniCPM profile must launch with a bounded context");
        check(manager.get_max_context_for_model("MiniCPM5:2b") == 8192,
              "the medium MiniCPM model also needs an explicit context");
        manager.set_max_context_override("MiniCPM5:1b", 4096);
        check(manager.get_max_context_for_model("MiniCPM5:1b") == 4096, "an explicit smaller context must be honored");
        manager.set_max_context_override("MiniCPM5:2b", 16384);
    }
    {
        delta::ModelManager manager;
        check(manager.get_max_context_for_model("MiniCPM5:2b") == 16384,
              "a saved larger context must survive restarting the manager");
        check(manager.get_max_context_for_model("not-a-registered-model") == 0,
              "unknown models must retain their existing resolution behavior");
    }
    return failures ? 1 : 0;
}
