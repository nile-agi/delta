#include "tool_documents.h"
#include "document_store.h"
#include "tool_registry.h"
#include "tool_task.h"
#include <algorithm>

namespace delta {
namespace agent {

namespace {

// Same cap as read_file's head-and-tail view, so one call cannot fill a small model's window.
constexpr size_t kMaxToolChars = 6000;

ToolResult not_found(const std::string& document) {
    std::string listed;
    for (const auto& d : DocumentStore::instance().list(active_memory_scope(), true))
        listed += (listed.empty() ? "" : ", ") + d.name;
    return {false, "",
            "No single attached file matches \"" + document + "\"." +
                (listed.empty() ? " No files are attached." : " Attached files: " + listed + ".")};
}

} // namespace

void register_document_tools() {
    auto& registry = ToolRegistry::instance();

    registry.register_tool(
        {"search_documents",
         "Search the files the user attached (PDF, Word, spreadsheets, text) and any they kept for all "
         "chats. Returns the best-matching passages with their file and page. Use it whenever the "
         "excerpts you were given do not answer the question, with different keywords if a first "
         "search misses. Quote the source like [report.pdf, p.12] when you use a passage.",
         {{"type", "object"},
          {"properties",
           {{"query", {{"type", "string"}, {"description", "Keywords to look for: names, terms, numbers"}}},
            {"document", {{"type", "string"}, {"description", "Optional file name to search only that file"}}}}},
          {"required", {"query"}}},
         ToolRisk::Safe,
         "documents"},
        [](const nlohmann::json& args) -> ToolResult {
            auto& store = DocumentStore::instance();
            const std::string cid = active_memory_scope();
            std::string doc_id;
            const std::string document = args.value("document", "");
            if (!document.empty()) {
                DocumentInfo info;
                if (!store.resolve(document, cid, info))
                    return not_found(document);
                doc_id = info.id;
            }
            auto hits = store.search(args.value("query", ""), cid, true, kMaxToolChars, 8, doc_id);
            nlohmann::json results = nlohmann::json::array();
            for (const auto& h : hits)
                results.push_back(h.to_json());
            nlohmann::json out = {{"results", results}};
            if (hits.empty())
                out["note"] = "Nothing matched. Try other keywords, or read_document to read a page range.";
            return {true, out.dump(), ""};
        });

    registry.register_tool(
        {"read_document",
         "Read an attached file in order: a range of pages, or of sections for files without pages. "
         "Use it to read around a passage search_documents found, or to go through a file from the start.",
         {{"type", "object"},
          {"properties",
           {{"document", {{"type", "string"}, {"description", "File name (or part of it)"}}},
            {"from", {{"type", "integer"}, {"description", "First page or section, starting at 1"}}},
            {"to", {{"type", "integer"}, {"description", "Last page or section (defaults to from)"}}}}},
          {"required", {"document"}}},
         ToolRisk::Safe,
         "documents"},
        [](const nlohmann::json& args) -> ToolResult {
            auto& store = DocumentStore::instance();
            const std::string document = args.value("document", "");
            DocumentInfo info;
            if (!store.resolve(document, active_memory_scope(), info))
                return not_found(document);
            int from = std::max(1, args.value("from", 1));
            int to = std::max(from, args.value("to", from));
            bool truncated = false;
            auto ex = store.read(info.id, from, to, kMaxToolChars, truncated);
            const int last = info.pages > 0 ? info.pages : info.chunks;
            nlohmann::json out = {{"source", ex.citation()}, {"text", ex.text}, {"total", last}};
            out["unit"] = info.pages > 0 ? "pages" : "sections";
            if (truncated)
                out["note"] = "Cut short to fit. Read a smaller range to see the rest.";
            else if (to < last)
                out["next"] = to + 1;
            return {true, out.dump(), ""};
        });

    registry.register_tool(
        {"list_documents",
         "List the files attached to this chat and those kept for all chats, with page counts.",
         {{"type", "object"}, {"properties", nlohmann::json::object()}, {"required", nlohmann::json::array()}},
         ToolRisk::Safe,
         "documents"},
        [](const nlohmann::json&) -> ToolResult {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& d : DocumentStore::instance().list(active_memory_scope(), true))
                arr.push_back(d.to_json());
            return {true, nlohmann::json({{"documents", arr}}).dump(), ""};
        });
}

} // namespace agent
} // namespace delta
