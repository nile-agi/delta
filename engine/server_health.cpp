#include "server_health.h"

#include "vendor/llama.cpp/vendor/cpp-httplib/httplib.h"

namespace delta {

bool probe_server_health(int port, int timeout_seconds) {
    try {
        httplib::Client cli("127.0.0.1", port);
        cli.set_connection_timeout(timeout_seconds, 0);
        cli.set_read_timeout(timeout_seconds, 0);
        auto res = cli.Get("/health");
        return res && res->status == 200;
    } catch (...) {
        return false;
    }
}

const char* to_string(ServerReadyState state) {
    switch (state) {
    case ServerReadyState::Ready:
        return "ready";
    case ServerReadyState::StillLoading:
        return "still loading";
    case ServerReadyState::Exited:
        return "exited";
    }
    return "unknown";
}

} // namespace delta
