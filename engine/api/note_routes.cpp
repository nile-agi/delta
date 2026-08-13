/**
 * Note API Routes for Delta
 *
 * This is a self-contained translation unit. Add both note_routes.cpp and
 * note_routes.h to your build, then call register_note_routes() from
 * model_api_server.cpp::setup_routes().
 *
 * In model_api_server.cpp:
 *   #include "api/note_routes.h"
 *   ...
 *   delta::api::register_note_routes(*server_);
 */

#include "note_routes.h"

#include <cpp-httplib/httplib.h>   // same path used by model_api_server.cpp
#include <nlohmann/json.hpp>
#include "../agent/agent_database.h"

using json = nlohmann::json;
using namespace delta::agent;

namespace delta {
namespace api {

// GET /api/notes
static void handle_list_notes(const httplib::Request& req, httplib::Response& res) {
  auto& db = AgentDatabase::instance();

  std::string search = req.has_param("search") ? req.get_param_value("search") : "";
  std::string folder = req.has_param("folder") ? req.get_param_value("folder") : "";
  std::string tags   = req.has_param("tags")   ? req.get_param_value("tags")   : "";
  int limit = 50;
  if (req.has_param("limit")) {
    try { limit = std::stoi(req.get_param_value("limit")); } catch (...) {}
  }
  bool pinned_only = req.has_param("pinned_only") && req.get_param_value("pinned_only") == "true";

  auto notes = db.list_notes(folder, search, tags, limit, pinned_only);
  json result = {{"notes", json::array()}, {"count", notes.size()}};
  for (auto& n : notes) result["notes"].push_back(n);
  res.set_content(result.dump(), "application/json");
}

// POST /api/notes
static void handle_create_note(const httplib::Request& req, httplib::Response& res) {
  try {
    auto& db = AgentDatabase::instance();
    json body = json::parse(req.body);

    if (!body.contains("title") || !body.contains("content")) {
      res.status = 400;
      res.set_content(json({{"error", "title and content are required"}}).dump(), "application/json");
      return;
    }

    std::string id = db.create_note(body);
    if (id.empty()) {
      res.status = 500;
      res.set_content(json({{"error", "Failed to create note"}}).dump(), "application/json");
      return;
    }

    res.set_content(db.get_note(id).dump(), "application/json");
  } catch (const std::exception& e) {
    res.status = 400;
    res.set_content(json({{"error", e.what()}}).dump(), "application/json");
  }
}

// GET /api/notes/:id
static void handle_get_note(const httplib::Request& req, httplib::Response& res) {
  auto& db = AgentDatabase::instance();
  std::string id = req.matches[1];

  auto note = db.get_note(id);
  if (note.is_null()) {
    res.status = 404;
    res.set_content(json({{"error", "Note not found"}}).dump(), "application/json");
    return;
  }
  res.set_content(note.dump(), "application/json");
}

// PUT /api/notes/:id
static void handle_update_note(const httplib::Request& req, httplib::Response& res) {
  try {
    auto& db = AgentDatabase::instance();
    std::string id = req.matches[1];
    json body = json::parse(req.body);

    if (!db.update_note(id, body)) {
      res.status = 404;
      res.set_content(json({{"error", "Note not found"}}).dump(), "application/json");
      return;
    }

    res.set_content(db.get_note(id).dump(), "application/json");
  } catch (const std::exception& e) {
    res.status = 400;
    res.set_content(json({{"error", e.what()}}).dump(), "application/json");
  }
}

// DELETE /api/notes/:id
static void handle_delete_note(const httplib::Request& req, httplib::Response& res) {
  auto& db = AgentDatabase::instance();
  std::string id = req.matches[1];

  if (!db.delete_note(id)) {
    res.status = 404;
    res.set_content(json({{"error", "Note not found"}}).dump(), "application/json");
    return;
  }
  res.set_content(json({{"deleted", true}}).dump(), "application/json");
}

// Register all note routes on a cpp-httplib server
void register_note_routes(httplib::Server& server) {
  server.Get("/api/notes", handle_list_notes);
  server.Post("/api/notes", handle_create_note);
  server.Get(R"(/api/notes/(.+))", handle_get_note);
  server.Put(R"(/api/notes/(.+))", handle_update_note);
  server.Delete(R"(/api/notes/(.+))", handle_delete_note);
}

} // namespace api
} // namespace delta
