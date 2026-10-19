#pragma once

#include <string>
#include "model_runtime.h"

namespace delta {

/**
 * Readiness of a spawned llama-server.
 *
 * llama-server binds its port before it starts loading weights and answers every request with
 * 503 "Loading model" until the model is resident, so a successful TCP connect only proves that
 * a process is holding the port -- it can even be a leftover server from a previous run.
 * A healthy router is only available; each selected worker needs its own status and props check.
 */
enum class ServerReadyState {
    Ready,        // /health returned 200: process available (not proof of router worker readiness)
    StillLoading, // process is alive but has not finished loading within the timeout
    Exited,       // process is gone; the load failed
};

/** True when llama-server on this port answers GET /health with 200. */
bool probe_server_health(int port, int timeout_seconds = 2);

const char* to_string(ServerReadyState state);
WorkerProbeResult probe_model_worker(int port, const RuntimeConfig&, bool router);
bool request_model_load(int port, const RuntimeConfig&, bool router);

} // namespace delta
