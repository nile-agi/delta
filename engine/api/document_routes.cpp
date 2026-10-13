#include "document_routes.h"

#include <chrono>
#include <cpp-httplib/httplib.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include "../agent/document_store.h"

using json = nlohmann::json;
using delta::agent::DocumentStore;

namespace delta {
namespace api {

namespace {

// Text, not file bytes: the client extracts before uploading. 50 MB of text is several
// thousand pages, well past anything a person attaches to a chat.
constexpr size_t kMaxUploadBytes = 50u * 1024 * 1024;

void send_error(httplib::Response& res, int status, const std::string& message) {
    res.status = status;
    res.set_content(json({{"error", message}}).dump(), "application/json");
}

std::string clip_id(std::string id) {
    if (id.size() > 128)
        id.resize(128);
    return id;
}

json documents_json(const std::vector<agent::DocumentInfo>& docs) {
    json arr = json::array();
    for (const auto& d : docs)
        arr.push_back(d.to_json());
    return arr;
}

// POST /api/documents
// {name, mime, conversation_id, library?, segments: [{page?, label?, text, records?}]}
void handle_upload(const httplib::Request& req, httplib::Response& res) {
    if (req.body.size() > kMaxUploadBytes)
        return send_error(res, 413, "This file has too much text to index.");
    auto& store = DocumentStore::instance();
    if (!store.ready())
        return send_error(res, 503, "Document search is unavailable.");
    try {
        json body = json::parse(req.body);
        const std::string name = body.value("name", "");
        if (name.empty() || !body.contains("segments") || !body["segments"].is_array())
            return send_error(res, 400, "name and segments are required");

        std::vector<agent::DocumentSegment> segments;
        for (const auto& s : body["segments"]) {
            if (!s.is_object() || !s.contains("text") || !s["text"].is_string())
                continue;
            agent::DocumentSegment seg;
            seg.page = std::max(0, s.value("page", 0));
            seg.label = s.value("label", "");
            seg.text = s["text"].get<std::string>();
            seg.records = s.value("records", false);
            if (!seg.text.empty())
                segments.push_back(std::move(seg));
        }
        if (segments.empty())
            return send_error(res, 400, "The file has no text to index.");

        auto t0 = std::chrono::steady_clock::now();
        const std::string id = store.add(name, body.value("mime", ""), clip_id(body.value("conversation_id", "")),
                                         segments, body.value("library", false));
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        if (id.empty())
            return send_error(res, 500, "Indexing failed.");

        agent::DocumentInfo info;
        store.resolve(id, clip_id(body.value("conversation_id", "")), info);
        std::cerr << "[delta-documents] indexed " << name << ": " << info.chunks << " chunks, " << info.chars
                  << " chars in " << ms << " ms" << std::endl;
        res.status = 201;
        res.set_content(json({{"document", info.to_json()}}).dump(), "application/json");
    } catch (const std::exception& e) {
        send_error(res, 400, e.what());
    }
}

// GET /api/documents?conversation_id=...&library=1
void handle_list(const httplib::Request& req, httplib::Response& res) {
    auto& store = DocumentStore::instance();
    const std::string cid = clip_id(req.has_param("conversation_id") ? req.get_param_value("conversation_id") : "");
    const bool library = req.has_param("library") && req.get_param_value("library") != "0";
    auto docs =
        cid.empty() ? (library ? store.library() : std::vector<agent::DocumentInfo>{}) : store.list(cid, library);
    res.set_content(json({{"documents", documents_json(docs)}}).dump(), "application/json");
}

// PUT /api/documents/:id  {library?: bool, attach?: conversation_id}
void handle_update(const httplib::Request& req, httplib::Response& res) {
    auto& store = DocumentStore::instance();
    const std::string id = req.matches[1];
    try {
        json body = json::parse(req.body);
        bool ok = true;
        if (body.contains("attach") && body["attach"].is_string())
            ok = store.attach(id, clip_id(body["attach"].get<std::string>())) && ok;
        if (body.contains("library") && body["library"].is_boolean())
            ok = store.set_library(id, body["library"].get<bool>()) && ok;
        if (!ok)
            return send_error(res, 404, "Document not found");
        res.set_content(json({{"updated", true}}).dump(), "application/json");
    } catch (const std::exception& e) {
        send_error(res, 400, e.what());
    }
}

// DELETE /api/documents/:id
void handle_delete(const httplib::Request& req, httplib::Response& res) {
    if (!DocumentStore::instance().remove(req.matches[1]))
        return send_error(res, 404, "Document not found");
    res.set_content(json({{"deleted", true}}).dump(), "application/json");
}

// DELETE /api/documents?conversation_id=...  (the chat was deleted)
void handle_delete_conversation(const httplib::Request& req, httplib::Response& res) {
    const std::string cid = clip_id(req.has_param("conversation_id") ? req.get_param_value("conversation_id") : "");
    if (cid.empty())
        return send_error(res, 400, "conversation_id is required");
    int removed = DocumentStore::instance().remove_conversation(cid);
    res.set_content(json({{"removed", removed}}).dump(), "application/json");
}

// POST /api/documents/search  {query, conversation_id, max_chars?}
void handle_search(const httplib::Request& req, httplib::Response& res) {
    try {
        json body = json::parse(req.body);
        size_t max_chars = static_cast<size_t>(std::max(500, std::min(50000, body.value("max_chars", 6000))));
        auto hits = DocumentStore::instance().search(body.value("query", ""),
                                                     clip_id(body.value("conversation_id", "")), true, max_chars);
        json arr = json::array();
        for (const auto& h : hits)
            arr.push_back(h.to_json());
        res.set_content(json({{"results", arr}}).dump(), "application/json");
    } catch (const std::exception& e) {
        send_error(res, 400, e.what());
    }
}

// GET /api/documents/:id/sections?conversation_id=...  (the whole text, for the viewer)
void handle_sections(const httplib::Request& req, httplib::Response& res) {
    auto& store = DocumentStore::instance();
    const std::string cid = clip_id(req.has_param("conversation_id") ? req.get_param_value("conversation_id") : "");
    agent::DocumentInfo info;
    if (!store.resolve(req.matches[1], cid, info) || info.id != req.matches[1].str())
        return send_error(res, 404, "Document not found");
    json sections = json::array();
    int number = 0;
    for (const auto& c : store.sections(info.id)) {
        json s = {{"section", ++number}, {"text", c.text}};
        if (c.page_start > 0) {
            s["page_start"] = c.page_start;
            s["page_end"] = std::max(c.page_start, c.page_end);
        }
        if (!c.heading.empty())
            s["heading"] = c.heading;
        sections.push_back(s);
    }
    res.set_content(json({{"document", info.to_json()}, {"sections", sections}}).dump(), "application/json");
}

} // namespace

void register_document_routes(httplib::Server& server) {
    server.Post("/api/documents/search", handle_search);
    server.Post("/api/documents", handle_upload);
    server.Get("/api/documents", handle_list);
    server.Delete("/api/documents", handle_delete_conversation);
    server.Get(R"(/api/documents/([A-Za-z0-9-]+)/sections)", handle_sections);
    server.Put(R"(/api/documents/([A-Za-z0-9-]+))", handle_update);
    server.Delete(R"(/api/documents/([A-Za-z0-9-]+))", handle_delete);
}

} // namespace api
} // namespace delta
