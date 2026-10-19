#include "runtime_config.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace delta;
int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    int failed = 0;
    auto reject = [&](auto fn) {
        try {
            fn();
            ++failed;
        } catch (const std::invalid_argument&) {
        }
    };
    RuntimeConfig c;
    c.context = 2048;
    c.parallel = 1;
    validate_runtime_config(c);
    c.parallel = -1;
    reject([&] { validate_runtime_config(c); });
    c.parallel = 4;
    c.context = 1024;
    reject([&] { validate_runtime_config(c); });
    c.context = 8192;
    c.batch = 64;
    c.microbatch = 128;
    reject([&] { validate_runtime_config(c); });
    {
        std::ofstream out(argv[1], std::ios::binary);
        out << "GGUF";
    }
    reject([&] { validate_model_file(argv[1]); });
    {
        std::ofstream out(argv[1], std::ios::binary);
        out.write("GGUF\3\0\0\0", 8);
        uint64_t n = 1, z = 0;
        out.write((char*)&n, 8);
        out.write((char*)&z, 8);
        out << std::string(160, '\0');
    }
    validate_model_file(argv[1]);
    reject([&] { validate_model_file(argv[1], 1000); });
    std::cout << "Runtime numeric bounds and GGUF preflight failures: " << failed << '\n';
    return failed ? 1 : 0;
}
