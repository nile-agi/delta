#include "owned_process.h"
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <stdexcept>
#ifndef _WIN32
#include <unistd.h>
#include <signal.h>
#endif
using namespace delta;
using namespace std::chrono_literals;
int main(int argc, char** argv) {
#ifndef _WIN32
    if (argc > 1 && std::string(argv[1]) == "--pipe-holder") {
        const auto pid = fork();
        if (pid == 0) {
            setsid();
            std::cerr << "holder:" << getpid() << '\n';
            std::this_thread::sleep_for(5s);
            _exit(0);
        }
        std::this_thread::sleep_for(100ms);
        return 0;
    }
#endif
    if (argc > 1 && std::string(argv[1]) == "--exit") {
        std::cerr << "loader: version `GLIBCXX_3.4.30' not found\n";
        return 7;
    }
    if (argc > 1 && std::string(argv[1]) == "--hold") {
        std::cerr << "child ready\n";
        while (true)
            std::this_thread::sleep_for(20ms);
    }
    if (argc > 1 && std::string(argv[1]) == "--args") {
        std::cerr << argv[2] << '\n';
        return 0;
    }
    try {
        std::mutex mutex;
        std::vector<std::string> output;
        ProcessExit exit;
        auto on_output = [&](uint64_t gen, const std::string& line) {
            std::lock_guard<std::mutex> lock(mutex);
            if (gen != 12)
                throw std::runtime_error("wrong generation");
            output.push_back(line);
        };
        auto on_exit = [&](uint64_t gen, ProcessExit result) {
            if (gen == 12)
                exit = result;
        };
        OwnedProcess sentinel;
        sentinel.start({{argv[0], "--hold"}, "", 11}, [](auto, auto) {}, [](auto, auto) {});
        OwnedProcess child;
        child.start({{argv[0], "--exit"}, "", 12}, on_output, on_exit);
        auto deadline = std::chrono::steady_clock::now() + 3s;
        while (child.alive() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(10ms);
        child.stop();
        if (exit.code != 7 || exit.signal != 0)
            throw std::runtime_error("exit evidence lost");
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (output.empty() || output.back().find("GLIBCXX_3.4.30") == std::string::npos)
                throw std::runtime_error("last stderr lost");
        }
        if (!sentinel.alive())
            throw std::runtime_error("unrelated process was stopped");
        child.stop();
        child.start({{argv[0], "--args", "path with spaces ' quotes \" and $characters"}, "", 12}, on_output, on_exit);
        while (child.alive())
            std::this_thread::sleep_for(10ms);
        child.stop();
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (output.back() != "path with spaces ' quotes \" and $characters")
                throw std::runtime_error("arguments altered");
        }
#ifndef _WIN32
        child.start({{argv[0], "--pipe-holder"}, "", 12}, on_output, on_exit);
        while (child.alive())
            std::this_thread::sleep_for(10ms);
        auto drain_start = std::chrono::steady_clock::now();
        child.stop();
        bool holder_found = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& line : output)
                if (line.rfind("holder:", 0) == 0) {
                    kill(std::stoi(line.substr(7)), SIGKILL);
                    holder_found = true;
                }
        }
        if (!holder_found || std::chrono::steady_clock::now() - drain_start > 1s)
            throw std::runtime_error("inherited pipe stalled shutdown");
#endif
        auto start = std::chrono::steady_clock::now();
        sentinel.stop();
        if (std::chrono::steady_clock::now() - start > 3s)
            throw std::runtime_error("shutdown exceeded bound");
        std::cout << "Owned child isolation, final stderr, exit status, argument and bounded stop checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
