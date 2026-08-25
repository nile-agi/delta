#pragma once

#include <string>

namespace delta {

/**
 * Readiness of a spawned llama-server.
 *
 * llama-server binds its port before it starts loading weights and answers every request with
 * 503 "Loading model" until the model is resident, so a successful TCP connect only proves that
 * a process is holding the port -- it can even be a leftover server from a previous run.
 * GET /health is the first 200 it serves, which makes it the only trustworthy "usable now" signal.
 */
enum class ServerReadyState {
    Ready,        // /health returned 200: the model is loaded and serving
    StillLoading, // process is alive but has not finished loading within the timeout
    Exited,       // process is gone; the load failed
};

/** True when llama-server on this port answers GET /health with 200. */
bool probe_server_health(int port, int timeout_seconds = 2);

const char* to_string(ServerReadyState state);

} // namespace delta
