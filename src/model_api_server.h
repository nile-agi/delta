/**
 * Model Management API Server Header
 */

#ifndef DELTA_MODEL_API_SERVER_H
#define DELTA_MODEL_API_SERVER_H

namespace delta {
    void start_model_api_server(int port = 8081);
    void stop_model_api_server();
}

#endif // DELTA_MODEL_API_SERVER_H

