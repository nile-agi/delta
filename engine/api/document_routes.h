#ifndef DELTA_API_DOCUMENT_ROUTES_H
#define DELTA_API_DOCUMENT_ROUTES_H

namespace httplib {
class Server;
}

namespace delta {
namespace api {

// /api/documents: upload extracted text for indexing, list, keep for all chats, delete, search.
void register_document_routes(httplib::Server& server);

} // namespace api
} // namespace delta

#endif // DELTA_API_DOCUMENT_ROUTES_H
