#ifndef DELTA_API_NOTE_ROUTES_H
#define DELTA_API_NOTE_ROUTES_H

// Forward-declare httplib::Server so we don't need to include httplib.h here.
// The actual include lives in note_routes.cpp where the class definition is needed.
namespace httplib {
class Server;
}

namespace delta {
namespace api {

void register_note_routes(httplib::Server& server);

} // namespace api
} // namespace delta

#endif // DELTA_API_NOTE_ROUTES_H
