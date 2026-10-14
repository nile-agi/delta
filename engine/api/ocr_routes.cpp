#include "ocr_routes.h"

#include <cpp-httplib/httplib.h>
#include <nlohmann/json.hpp>
#include "../agent/ocr_reader.h"

using json = nlohmann::json;
using delta::agent::OcrReader;

namespace delta {
namespace api {

namespace {

// One page image; a 300 DPI A4 PNG is well under this.
constexpr size_t kMaxPageBytes = 25u * 1024 * 1024;

void send_error(httplib::Response& res, int status, const std::string& message) {
    res.status = status;
    res.set_content(json({{"error", message}}).dump(), "application/json");
}

void handle_status(const httplib::Request&, httplib::Response& res) {
    res.set_content(OcrReader::instance().status().dump(), "application/json");
}

void handle_download(const httplib::Request&, httplib::Response& res) {
    auto& reader = OcrReader::instance();
    if (!reader.installed() && !reader.start_download())
        res.status = 409; // already downloading
    res.set_content(reader.status().dump(), "application/json");
}

void handle_cancel(const httplib::Request&, httplib::Response& res) {
    OcrReader::instance().cancel_download();
    res.set_content(OcrReader::instance().status().dump(), "application/json");
}

// POST /api/ocr/page {image: "data:image/png;base64,..." or bare base64, mime?}
void handle_page(const httplib::Request& req, httplib::Response& res) {
    if (req.body.size() > kMaxPageBytes)
        return send_error(res, 413, "This page image is too large.");
    std::string image;
    try {
        const json body = json::parse(req.body);
        image = body.value("image", "");
        if (!image.empty() && image.rfind("data:", 0) != 0)
            image = "data:" + body.value("mime", std::string("image/png")) + ";base64," + image;
    } catch (const std::exception&) {
        return send_error(res, 400, "Invalid JSON in request body");
    }
    if (image.empty())
        return send_error(res, 400, "image is required");
    const auto page = OcrReader::instance().read_page(image);
    if (page.status != 200)
        return send_error(res, page.status, page.error);
    res.set_content(json({{"text", page.text}}).dump(), "application/json");
}

} // namespace

void register_ocr_routes(httplib::Server& server) {
    server.Get("/api/ocr", handle_status);
    server.Post("/api/ocr/download", handle_download);
    server.Post("/api/ocr/download/cancel", handle_cancel);
    server.Post("/api/ocr/page", handle_page);
}

} // namespace api
} // namespace delta
