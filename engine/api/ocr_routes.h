#ifndef DELTA_API_OCR_ROUTES_H
#define DELTA_API_OCR_ROUTES_H

namespace httplib {
class Server;
}

namespace delta {
namespace api {

// /api/ocr: the text reader for scanned pages (status, download, cancel, read a page).
void register_ocr_routes(httplib::Server& server);

} // namespace api
} // namespace delta

#endif // DELTA_API_OCR_ROUTES_H
