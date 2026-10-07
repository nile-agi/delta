#ifndef DELTA_DOCUMENT_STORE_H
#define DELTA_DOCUMENT_STORE_H

#include <mutex>
#include <string>
#include <vector>
#include "json.hpp"
#include "sqlite3.h"

namespace delta {
namespace agent {

// One piece of an uploaded file as the client extracted it: a PDF page, a sheet, or the whole
// text of a document that has no pages.
struct DocumentSegment {
    int page = 0;      // 1-based page number; 0 when the file has no pages
    std::string label; // sheet name, or empty
    std::string text;
    // Each line is a self-contained record (a spreadsheet row). Records are never split and
    // chunks of them get no overlap, because a row cut in half means nothing.
    bool records = false;
};

// A retrievable slice of a document, as the chunker produces it.
struct DocumentChunk {
    int page_start = 0;
    int page_end = 0;
    std::string heading;
    std::string text;
    // Characters at the start of `text` repeated from the previous chunk, so neighbouring chunks
    // can be stitched back together without saying the same sentence twice.
    int overlap = 0;
};

struct DocumentInfo {
    std::string id;
    std::string name;
    std::string mime;
    std::string scope; // conversation | library
    int pages = 0;
    int chars = 0;
    int chunks = 0;
    std::string created_at;

    nlohmann::json to_json() const;
};

// Neighbouring chunks that matched a query, merged into one passage.
struct DocumentExcerpt {
    std::string doc_id;
    std::string doc_name;
    int page_start = 0;
    int page_end = 0;
    int section_start = 0; // 1-based chunk number, for files without pages
    int section_end = 0;
    std::string heading;
    std::string text;
    double score = 0; // bm25: lower is better

    // "report.pdf, p.12-13" or "notes.md, section 4".
    std::string citation() const;
    nlohmann::json to_json() const;
};

// Splits extracted text into chunks of roughly `target` characters on paragraph and sentence
// boundaries, carrying `overlap` characters from each chunk into the next. Exposed for tests.
std::vector<DocumentChunk> chunk_segments(const std::vector<DocumentSegment>& segments, size_t target = 1800,
                                          size_t overlap = 200);

// Turns free text into an FTS5 query that cannot be a syntax error: every term quoted, joined
// with OR, stop words dropped. Empty when nothing searchable is left. Exposed for tests.
std::string build_match_query(const std::string& text);

// Files the user attached, cut into chunks and indexed with SQLite FTS5, so the model is handed
// the passages that answer the question instead of the whole file.
//
// A document is linked to every conversation it was attached in, and is deleted when its last
// link goes, unless the user kept it for all chats (scope "library"), in which case it stays
// searchable from any conversation.
class DocumentStore {
  public:
    static DocumentStore& instance();

    // Creates the tables if needed. `mutex` guards `db` for every caller sharing the connection;
    // null gives the store a lock of its own. False when this SQLite build lacks FTS5.
    bool init(sqlite3* db, std::recursive_mutex* mutex = nullptr);
    bool ready() const { return db_ != nullptr; }
    void detach();

    // Chunks and indexes the file in one transaction. Returns the new id, or "" on failure.
    std::string add(const std::string& name, const std::string& mime, const std::string& conversation_id,
                    const std::vector<DocumentSegment>& segments, bool library = false);
    bool attach(const std::string& doc_id, const std::string& conversation_id);
    bool set_library(const std::string& doc_id, bool library);
    bool remove(const std::string& doc_id);
    // Unlinks every document from the conversation and deletes the ones nothing else holds.
    int remove_conversation(const std::string& conversation_id);

    // Documents attached to the conversation, plus the library when `include_library` is set.
    std::vector<DocumentInfo> list(const std::string& conversation_id, bool include_library) const;
    std::vector<DocumentInfo> library() const;
    bool has_documents(const std::string& conversation_id) const;
    // Finds an accessible document by id, exact name, or a unique part of its name.
    bool resolve(const std::string& id_or_name, const std::string& conversation_id, DocumentInfo& out) const;

    // Best passages for `query`, at most `max_chars` of text in total. `doc_id` narrows the search
    // to one document.
    std::vector<DocumentExcerpt> search(const std::string& query, const std::string& conversation_id,
                                        bool include_library, size_t max_chars, int max_hits = 8,
                                        const std::string& doc_id = "") const;
    // The text of pages [from, to] (or sections, for a file without pages), capped at `max_chars`.
    // `truncated` reports whether the range had more to give.
    DocumentExcerpt read(const std::string& doc_id, int from, int to, size_t max_chars, bool& truncated) const;

  private:
    DocumentStore() = default;
    DocumentStore(const DocumentStore&) = delete;
    DocumentStore& operator=(const DocumentStore&) = delete;

    bool delete_unlocked(const std::string& doc_id);

    sqlite3* db_ = nullptr;
    std::recursive_mutex own_mutex_;
    mutable std::recursive_mutex* mutex_ = &own_mutex_;
};

} // namespace agent
} // namespace delta

#endif // DELTA_DOCUMENT_STORE_H
