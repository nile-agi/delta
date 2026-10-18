#pragma once
#include <string>
namespace delta {
struct RuntimeFailure {
    std::string code, message;
    int exit_code = 0;
    int signal = 0;
};
} // namespace delta
