#pragma once

#include <string>
#include <vector>

namespace delta::background {

// `delta server start` / `delta --server --detach`: runs `delta --server <args>` detached, with its
// output in ~/.delta-cli/logs/server.log and its pid and port in ~/.delta-cli/server.json.
int start(const std::vector<std::string>& server_args);
int stop();
int status();
int logs();

// The web UI port of a background server that is up and answering, or 0.
int running_port();

} // namespace delta::background
