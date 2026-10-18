#pragma once
#include <string>
#include "runtime_config.h"
namespace delta {
struct RuntimeFailure {
    std::string code, message;
    int exit_code = 0;
    int signal = 0;
    std::string action = "retry";
    int suggested_context = 0;
};
struct ProcessEvidence {
    std::string errors;
    int exit_code = 0;
    int signal = 0;
};
enum class RecoveryAction { Stop, RetryCpu };
RuntimeFailure classify_runtime_failure(const ProcessEvidence&);
RecoveryAction recovery_action(const RuntimeFailure&, const RuntimeConfig&, unsigned retries_used);
} // namespace delta
