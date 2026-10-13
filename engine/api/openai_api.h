#ifndef DELTA_API_OPENAI_API_H
#define DELTA_API_OPENAI_API_H

#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace httplib {
class Server;
struct Request;
struct Response;
} // namespace httplib

namespace delta {
namespace api {

// Who may reach the model API: saved in ~/.delta-cli/api_server.json, shared by the CLI and the app.
struct ApiAccessSettings {
    bool network = false; // listen on every interface instead of this computer only
    std::string api_key;  // required on /v1/* when set; always set while network is on
};

class ApiAccess {
  public:
    explicit ApiAccess(std::string path = "") : path_(std::move(path)) {}

    // Reads the saved settings; a missing or damaged file leaves the defaults.
    void load();
    // Writes the saved settings (owner-only permissions); false when the file cannot be written.
    bool save(const ApiAccessSettings& settings);

    // Flags for this run only (`--host`, `--api-key`); they win over the saved settings.
    void set_overrides(const std::string& host, const std::string& api_key);

    ApiAccessSettings saved() const;
    std::string host() const;
    std::string api_key() const;
    bool overridden() const;

  private:
    mutable std::mutex mutex_;
    std::string path_;
    ApiAccessSettings saved_;
    std::string host_override_;
    std::string key_override_;
};

struct Gate {
    int status = 0; // 0 lets the request through
    nlohmann::json body;
};

bool is_loopback(const std::string& address);
// The OpenAI routes other devices and tools may call; everything else stays on this computer.
bool is_openai_path(const std::string& path);
// An Origin a web page on this computer (or the desktop app) sends; empty means no browser.
bool trusted_origin(const std::string& origin);
// Decides whether a request may proceed before any route runs.
Gate check_request(const std::string& method, const std::string& path, const std::string& remote_addr,
                   const std::string& authorization, const std::string& x_api_key, const std::string& api_key);

// Delta's own chat fields; a request without them is a plain OpenAI call.
bool is_delta_request(const nlohmann::json& body);

nlohmann::json openai_error(const std::string& message, const std::string& type, const std::string& code = "");

// Forwards an OpenAI request to llama-server unchanged apart from `body`, streaming when asked.
// `reply_model`, when set, replaces the `model` llama-server reports (its file stem) in every reply.
void proxy_openai(httplib::Response& res, const std::string& path, const nlohmann::json& body, const std::string& host,
                  int port, const std::string& reply_model = "");

std::string generate_api_key();
std::string mask_api_key(const std::string& key);
// This computer's address on the local network, or empty when there is none.
std::string lan_address();

struct DeveloperRoutes {
    ApiAccess* access = nullptr;
    std::function<int()> port;    // the model API's port
    std::function<void()> rebind; // called after the listening address changed
};

// GET/PUT /api/developer/server: the API's address, network access and key, for the app.
void register_developer_routes(httplib::Server& server, DeveloperRoutes routes);

} // namespace api
} // namespace delta

#endif // DELTA_API_OPENAI_API_H
