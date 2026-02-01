/**
 * Model Management API Server Header
 */

#ifndef DELTA_MODEL_API_SERVER_H
#define DELTA_MODEL_API_SERVER_H

#include <string>
#include <functional>

namespace delta {
    void start_model_api_server(int port = 8081);
    void stop_model_api_server();
    
    // Callback function type for model switching
    // Parameters: model_path, model_name, ctx_size, model_alias
    using ModelSwitchCallback = std::function<bool(const std::string&, const std::string&, int, const std::string&)>;
    
    // Callback for unloading model / stopping llama-server
    using ModelUnloadCallback = std::function<void()>;
    
    // Set callback to be called when model switch is requested
    void set_model_switch_callback(ModelSwitchCallback callback);
    
    // Set callback to be called when model unload / stop server is requested
    void set_model_unload_callback(ModelUnloadCallback callback);
}

#endif // DELTA_MODEL_API_SERVER_H

