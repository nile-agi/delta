#include "server_health.h"
#include "vendor/json.hpp"
#include <filesystem>

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

bool request_model_load(int port, const RuntimeConfig& config, bool router) {
    if (!router)
        return true;
    httplib::Client cli("127.0.0.1", port);
    cli.set_connection_timeout(1, 0);
    cli.set_read_timeout(1, 0);
    cli.set_write_timeout(1, 0);
    const auto response =
        cli.Post("/models/load", nlohmann::json{{"model", config.model_alias}}.dump(), "application/json");
    return response && response->status == 200;
}
WorkerProbeResult probe_model_worker(int port, const RuntimeConfig& config, bool router) {
    using nlohmann::json;
    WorkerProbeResult result;
    httplib::Client cli("127.0.0.1", port);
    cli.set_connection_timeout(1, 0);
    cli.set_read_timeout(1, 0);
    auto fail = [&](const char* code, const char* message) {
        result.failed = true;
        result.failure = {code, message};
        return result;
    };
    try {
        if (router) {
            auto response = cli.Get("/models");
            if (!response || response->status != 200) {
                result.engine_available = false;
                return result;
            }
            auto body = json::parse(response->body);
            bool found = false;
            for (const auto& entry : body.at("data")) {
                if (entry.value("id", "") != config.model_alias)
                    continue;
                found = true;
                const auto status = entry.at("status");
                if (status.value("failed", false)) {
                    result.failure = {"worker_exit", "Model worker exited while loading.",
                                      status.value("exit_code", 0)};
                    result.failed = true;
                    return result;
                }
                if (status.value("value", "") != "loaded")
                    return result;
                break;
            }
            if (!found)
                return fail("model_identity", "Selected model is missing from the router.");
        }
        std::string path = "/props";
        if (router)
            path += "?model=" + httplib::encode_uri_component(config.model_alias) + "&autoload=false";
        auto props = cli.Get(path);
        if (!props || props->status == 503)
            return result;
        if (props->status != 200)
            return fail("worker_props", "Cannot verify the selected model configuration.");
        const auto body = json::parse(props->body);
        const int slots = body.at("total_slots").get<int>();
        const int context = body.at("default_generation_settings").at("n_ctx").get<int>();
        const auto identity =
            body.value("model_alias", std::filesystem::path(body.value("model_path", "")).stem().string());
        if (identity != config.model_alias)
            return fail("model_identity", "Worker identity does not match the selected model.");
        if (slots != config.parallel || context != config.context / config.parallel)
            return fail("worker_config", "Worker context or parallel slots differ from the requested configuration.");
        result.ready = true;
        return result;
    } catch (...) {
        return fail("worker_protocol", "Invalid model status or configuration response.");
    }
}

} // namespace delta
