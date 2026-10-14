#include "document_store.h"
#include "time_compat.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <cmath>
#include <iostream>
#include <map>
#include <limits>
#include <random>
#include <set>

namespace delta {
namespace agent {

namespace {

std::string new_id() {
    static std::mutex mtx;
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";
    std::lock_guard<std::mutex> lock(mtx);
    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    for (auto& c : uuid) {
        if (c == 'x')
            c = hex[dis(gen)];
        else if (c == 'y')
            c = hex[(dis(gen) & 0x3) | 0x8];
    }
    return uuid;
}

std::string utc_now() {
    time_t now = time(nullptr);
    struct tm t{};
    utc_time(&now, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
    return std::string(buf);
}

std::string col_text(sqlite3_stmt* stmt, int i) {
    const unsigned char* txt = sqlite3_column_text(stmt, i);
    return txt ? std::string(reinterpret_cast<const char*>(txt)) : "";
}

std::string lower(std::string s) {
    for (auto& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Moves `pos` back to the first byte of a UTF-8 character, so a cut never splits one.
size_t utf8_floor(const std::string& s, size_t pos) {
    if (pos >= s.size())
        return s.size();
    while (pos > 0 && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80)
        pos--;
    return pos;
}

bool is_sentence_end(const std::string& s, size_t i) {
    // A boundary sits just after i: ". ", "? ", "! " or a line break.
    if (s[i] == '\n')
        return true;
    if ((s[i] == '.' || s[i] == '?' || s[i] == '!') && i + 1 < s.size() && (s[i + 1] == ' ' || s[i + 1] == '\n'))
        return true;
    return false;
}

// The cut nearest `want` in [lo, hi]: a sentence end if there is one, else whitespace, else a
// hard cut on a character boundary. Returns the index the next piece starts at.
size_t best_cut(const std::string& s, size_t want, size_t lo, size_t hi) {
    hi = std::min(hi, s.size() - 1);
    size_t best = std::string::npos;
    size_t best_dist = std::string::npos;
    for (size_t i = lo; i <= hi; i++) {
        if (is_sentence_end(s, i)) {
            size_t d = i > want ? i - want : want - i;
            if (d < best_dist) {
                best = i + 1;
                best_dist = d;
            }
        }
    }
    if (best != std::string::npos)
        return best;
    for (size_t i = lo; i <= hi; i++) {
        if (s[i] == ' ' || s[i] == '\t') {
            size_t d = i > want ? i - want : want - i;
            if (d < best_dist) {
                best = i + 1;
                best_dist = d;
            }
        }
    }
    if (best != std::string::npos)
        return best;
    return std::max<size_t>(utf8_floor(s, want), lo);
}

std::vector<std::string> split_long(const std::string& text, size_t target) {
    std::vector<std::string> pieces;
    size_t pos = 0;
    const size_t slack = target / 5;
    while (text.size() - pos > target + slack) {
        size_t cut = best_cut(text, pos + target, pos + target - slack, pos + target + slack);
        if (cut <= pos)
            cut = utf8_floor(text, pos + target);
        pieces.push_back(trim(text.substr(pos, cut - pos)));
        pos = cut;
    }
    std::string rest = trim(text.substr(pos));
    if (!rest.empty())
        pieces.push_back(rest);
    return pieces;
}

// The last ~`overlap` characters of `text`, starting at a sentence or word so the carried-over
// part reads cleanly.
std::string tail_for_overlap(const std::string& text, size_t overlap) {
    if (overlap == 0 || text.size() <= overlap)
        return "";
    size_t start = text.size() - overlap;
    for (size_t i = start; i < text.size() - overlap / 2; i++) {
        if (is_sentence_end(text, i))
            return trim(text.substr(i + 1));
    }
    for (size_t i = start; i < text.size(); i++) {
        if (text[i] == ' ' || text[i] == '\n')
            return trim(text.substr(i + 1));
    }
    return trim(text.substr(utf8_floor(text, start)));
}

bool looks_like_heading(const std::string& para) {
    if (para.empty())
        return false;
    if (para[0] == '#')
        return true;
    if (para.size() > 80 || para.find('\n') != std::string::npos)
        return false;
    char last = para.back();
    if (last == '.' || last == ',' || last == ';' || last == ':' || last == '?' || last == '!')
        return false;
    bool has_alpha = false;
    for (char c : para)
        if (std::isalpha(static_cast<unsigned char>(c)))
            has_alpha = true;
    // Words that start lower case are a sentence fragment wrapped onto its own line, not a title.
    return has_alpha && !std::islower(static_cast<unsigned char>(para[0]));
}

std::string heading_text(const std::string& para) {
    size_t i = 0;
    while (i < para.size() && (para[i] == '#' || para[i] == ' '))
        i++;
    return para.substr(i);
}

struct Block {
    int page = 0;
    std::string label;
    std::string text;
    bool records = false;
    bool heading = false;
};

std::vector<Block> to_blocks(const std::vector<DocumentSegment>& segments, size_t target) {
    std::vector<Block> blocks;
    for (const auto& seg : segments) {
        if (seg.records) {
            size_t start = 0;
            while (start <= seg.text.size()) {
                size_t end = seg.text.find('\n', start);
                if (end == std::string::npos)
                    end = seg.text.size();
                std::string line = trim(seg.text.substr(start, end - start));
                if (!line.empty())
                    blocks.push_back({seg.page, seg.label, line, true, false});
                start = end + 1;
            }
            continue;
        }
        // Paragraphs are separated by a blank line.
        std::string norm;
        norm.reserve(seg.text.size());
        for (char c : seg.text)
            if (c != '\r')
                norm += c;
        size_t start = 0;
        while (start < norm.size()) {
            size_t end = norm.find("\n\n", start);
            if (end == std::string::npos)
                end = norm.size();
            std::string para = trim(norm.substr(start, end - start));
            start = end + 2;
            if (para.empty())
                continue;
            if (looks_like_heading(para)) {
                blocks.push_back({seg.page, seg.label, para, false, true});
            } else if (para.size() > target + target / 5) {
                for (auto& piece : split_long(para, target))
                    blocks.push_back({seg.page, seg.label, piece, false, false});
            } else {
                blocks.push_back({seg.page, seg.label, para, false, false});
            }
        }
    }
    return blocks;
}

} // namespace

namespace {

// Whitespace runs folded to single spaces, so a digest line is one line.
std::string fold_spaces(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    bool space = false;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            space = !out.empty();
            continue;
        }
        if (space)
            out += ' ';
        space = false;
        out += c;
    }
    return out;
}

// Up to `sentences` whole sentences of `text`, never longer than `max_chars`.
std::string first_sentences(const std::string& text, size_t max_chars, int sentences) {
    const std::string t = fold_spaces(text);
    size_t end = 0;
    int found = 0;
    for (size_t i = 0; i < t.size() && i < max_chars && found < sentences; i++) {
        const char c = t[i];
        if ((c == '.' || c == '?' || c == '!') && (i + 1 == t.size() || t[i + 1] == ' ')) {
            end = i + 1;
            found++;
        }
    }
    if (end > 0)
        return t.substr(0, end);
    if (t.size() <= max_chars)
        return t;
    size_t cut = utf8_floor(t, max_chars);
    const size_t space = t.rfind(' ', cut);
    if (space != std::string::npos && space > max_chars / 2)
        cut = space;
    return t.substr(0, cut) + "...";
}

std::string place_tag(int page_start, int page_end, size_t section_start, size_t section_end) {
    if (page_start > 0)
        return "[p." + std::to_string(page_start) + (page_end > page_start ? "-" + std::to_string(page_end) : "") + "]";
    return "[section " + std::to_string(section_start) +
           (section_end > section_start ? "-" + std::to_string(section_end) : "") + "]";
}

std::string chunk_body(const DocumentChunk& c) {
    const size_t skip = std::min(c.text.size(), static_cast<size_t>(std::max(0, c.overlap)));
    return c.text.substr(skip);
}

} // namespace

std::string build_digest(const std::vector<DocumentChunk>& chunks, size_t total_chars) {
    if (chunks.empty())
        return "";
    const size_t budget = std::clamp<size_t>(total_chars * 3 / 100, 800, 4000);

    // The opening: title, abstract, introduction, whatever the file starts with.
    const size_t opening_budget = budget * 2 / 5;
    std::string opening;
    size_t next = 0;
    while (next < chunks.size() && opening.size() < opening_budget)
        opening += " " + chunk_body(chunks[next++]);
    std::string out = place_tag(chunks[0].page_start, chunks[next - 1].page_end, 1, next) + " " +
                      first_sentences(opening, opening_budget, 1000) + "\n";

    // Then the most informative sentences of the rest: rare words and figures score high, text that
    // repeats on every page (headers, boilerplate) scores nothing. Kept in document order.
    struct Sentence {
        size_t chunk;
        std::string text;
        double score;
    };
    std::vector<std::vector<std::string>> chunk_words(chunks.size());
    std::map<std::string, int> df;
    std::vector<Sentence> sentences;
    for (size_t i = next; i < chunks.size(); i++) {
        const std::string body = fold_spaces(chunk_body(chunks[i]));
        std::set<std::string> seen;
        for (size_t at = 0; at < body.size();) {
            size_t end = at;
            while (end < body.size() && !((body[end] == '.' || body[end] == '?' || body[end] == '!') &&
                                          (end + 1 == body.size() || body[end + 1] == ' ')))
                end++;
            const std::string sentence = body.substr(at, std::min(body.size(), end + 1) - at);
            at = end + 2;
            if (sentence.size() < 40 || sentence.size() > 320)
                continue;
            sentences.push_back({i, sentence, 0});
            std::string word;
            for (char c : sentence + " ") {
                if (std::isalnum(static_cast<unsigned char>(c)) || (c & 0x80)) {
                    word += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                } else if (!word.empty()) {
                    if (word.size() > 2 && seen.insert(word).second)
                        df[word]++;
                    word.clear();
                }
            }
        }
    }
    const double n = static_cast<double>(std::max<size_t>(1, chunks.size() - next));
    for (auto& s : sentences) {
        std::set<std::string> words;
        std::string word;
        bool figure = false;
        for (char c : s.text + " ") {
            figure = figure || std::isdigit(static_cast<unsigned char>(c));
            if (std::isalnum(static_cast<unsigned char>(c)) || (c & 0x80))
                word += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            else if (!word.empty()) {
                if (word.size() > 2)
                    words.insert(word);
                word.clear();
            }
        }
        for (const auto& w : words)
            s.score += std::log(n / std::max(1, df[w]));
        s.score = s.score / std::sqrt(static_cast<double>(std::max<size_t>(1, words.size()))) + (figure ? 1.5 : 0);
    }
    std::vector<size_t> order(sentences.size());
    for (size_t i = 0; i < order.size(); i++)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&](size_t a, size_t b) { return sentences[a].score > sentences[b].score; });
    std::vector<size_t> picked;
    std::map<size_t, int> per_chunk;
    size_t used = out.size();
    auto take = [&](size_t i) {
        const auto& s = sentences[i];
        const size_t line = s.text.size() + 16;
        if (s.score <= 0 || per_chunk[s.chunk] >= 2 || used + line > budget ||
            std::find(picked.begin(), picked.end(), i) != picked.end())
            return;
        used += line;
        per_chunk[s.chunk]++;
        picked.push_back(i);
    };
    // The best sentence of each quarter first, so the whole file is covered, then the best overall.
    const size_t rest = chunks.size() - next;
    for (size_t q = 0; q < 4 && rest > 0; q++) {
        const size_t lo = next + q * rest / 4, hi = next + (q + 1) * rest / 4;
        for (size_t i : order)
            if (sentences[i].chunk >= lo && sentences[i].chunk < hi) {
                take(i);
                break;
            }
    }
    for (size_t i : order)
        take(i);
    std::sort(picked.begin(), picked.end());
    for (size_t i : picked) {
        const auto& c = chunks[sentences[i].chunk];
        out += place_tag(c.page_start, c.page_end, sentences[i].chunk + 1, sentences[i].chunk + 1) + " " +
               sentences[i].text + "\n";
    }
    return out;
}

std::vector<DocumentChunk> chunk_segments(const std::vector<DocumentSegment>& segments, size_t target, size_t overlap) {
    std::vector<DocumentChunk> out;
    if (target == 0)
        return out;
    const auto blocks = to_blocks(segments, target);

    std::string buf;
    size_t carried = 0; // characters of buf that came from the previous chunk
    int page_start = 0, page_end = 0;
    std::string chunk_heading, current_heading, chunk_label;
    bool chunk_records = false;

    auto has_new = [&]() { return buf.size() > carried; };
    auto flush = [&]() {
        if (!has_new())
            return;
        out.push_back({page_start, page_end, chunk_heading, buf, static_cast<int>(carried)});
        std::string tail = chunk_records ? "" : tail_for_overlap(buf, overlap);
        buf = tail;
        carried = tail.size();
        page_start = page_end;
    };

    for (const auto& b : blocks) {
        // A sheet's rows never share a chunk with another sheet or with prose.
        if (has_new() && (b.label != chunk_label || b.records != chunk_records))
            flush();
        if (b.label != chunk_label || b.records != chunk_records) {
            buf.clear();
            carried = 0;
            chunk_label = b.label;
            chunk_records = b.records;
            current_heading = b.label;
        }
        if (b.heading) {
            // Start a fresh chunk at a heading once the current one has some substance.
            if (has_new() && buf.size() - carried >= target / 2)
                flush();
            current_heading = heading_text(b.text);
        }
        // Prefer to end a reasonably full chunk where a page ends, so citations name few pages.
        if (has_new() && b.page != page_end && buf.size() - carried >= target * 3 / 5)
            flush();
        const char* sep = b.records ? "\n" : "\n\n";
        if (has_new() && buf.size() + strlen(sep) + b.text.size() > target)
            flush();
        if (!has_new()) {
            chunk_heading = current_heading;
            // Carried-over text keeps the page flush() gave it; a clean start begins here.
            if (carried == 0)
                page_start = b.page;
        }
        if (!buf.empty())
            buf += sep;
        buf += b.text;
        page_end = b.page;
    }
    flush();
    return out;
}

std::string build_match_query(const std::string& text) {
    static const std::set<std::string> stop = {
        "a",     "an",     "the",  "and",      "or",   "but",   "if",    "of",       "to",      "in",   "on",
        "at",    "by",     "for",  "with",     "from", "as",    "is",    "are",      "was",     "were", "be",
        "been",  "it",     "its",  "this",     "that", "these", "those", "there",    "here",    "what", "which",
        "who",   "whom",   "when", "where",    "why",  "how",   "do",    "does",     "did",     "can",  "could",
        "would", "should", "will", "shall",    "may",  "might", "must",  "i",        "me",      "my",   "you",
        "your",  "we",     "our",  "they",     "them", "their", "he",    "she",      "his",     "her",  "not",
        "no",    "yes",    "so",   "than",     "then", "too",   "very",  "just",     "about",   "into", "over",
        "any",   "all",    "some", "has",      "have", "had",   "tell",  "please",   "explain", "give", "show",
        "say",   "says",   "said", "document", "file", "pdf",   "doc",   "according"};
    std::vector<std::string> terms;
    std::set<std::string> seen;
    std::string cur;
    auto take = [&]() {
        if (cur.empty())
            return;
        bool digits =
            std::all_of(cur.begin(), cur.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
        if ((cur.size() >= 2 || digits) && !stop.count(cur) && !seen.count(cur) && terms.size() < 24) {
            seen.insert(cur);
            terms.push_back(cur);
        }
        cur.clear();
    };
    for (char raw : text) {
        unsigned char c = static_cast<unsigned char>(raw);
        // Bytes of a multi-byte UTF-8 character count as word characters, like FTS5's unicode61.
        if (std::isalnum(c) || c >= 0x80)
            cur += static_cast<char>(std::tolower(c));
        else
            take();
    }
    take();
    std::string q;
    for (const auto& t : terms) {
        if (!q.empty())
            q += " OR ";
        q += "\"" + t + "\"";
    }
    return q;
}

nlohmann::json DocumentInfo::to_json() const {
    return {{"id", id},       {"name", name},   {"mime", mime},     {"scope", scope},
            {"pages", pages}, {"chars", chars}, {"chunks", chunks}, {"created_at", created_at}};
}

std::string DocumentExcerpt::citation() const {
    std::string c = doc_name;
    if (page_start > 0) {
        c += ", p." + std::to_string(page_start);
        if (page_end > page_start)
            c += "-" + std::to_string(page_end);
    } else if (section_start > 0) {
        c += ", section " + std::to_string(section_start);
        if (section_end > section_start)
            c += "-" + std::to_string(section_end);
    }
    return c;
}

nlohmann::json DocumentExcerpt::to_json() const {
    nlohmann::json j = {{"document", doc_name}, {"document_id", doc_id}, {"source", citation()}, {"text", text}};
    if (!heading.empty())
        j["heading"] = heading;
    // Numbers as well as the citation string, so the app can open the file at this place.
    if (page_start > 0) {
        j["page_start"] = page_start;
        j["page_end"] = std::max(page_start, page_end);
    } else if (section_start > 0) {
        j["section_start"] = section_start;
        j["section_end"] = std::max(section_start, section_end);
    }
    return j;
}

DocumentStore& DocumentStore::instance() {
    static DocumentStore store;
    return store;
}

void DocumentStore::detach() {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    db_ = nullptr;
}

bool DocumentStore::init(sqlite3* db, std::recursive_mutex* mutex) {
    if (!db)
        return false;
    mutex_ = mutex ? mutex : &own_mutex_;
    std::lock_guard<std::recursive_mutex> lock(*mutex_);

    // Chunks keep an integer rowid so the FTS index can point at them (external content), which
    // stores the text once instead of twice. Chunks are never updated, only inserted and deleted,
    // so two triggers keep the index in step.
    const char* schema = R"(
      CREATE TABLE IF NOT EXISTS documents (
        id TEXT PRIMARY KEY,
        name TEXT NOT NULL,
        mime TEXT DEFAULT '',
        scope TEXT NOT NULL DEFAULT 'conversation' CHECK(scope IN ('conversation','library')),
        pages INTEGER DEFAULT 0,
        chars INTEGER DEFAULT 0,
        chunks INTEGER DEFAULT 0,
        created_at TEXT NOT NULL,
        digest TEXT DEFAULT ''
      );
      CREATE TABLE IF NOT EXISTS document_links (
        doc_id TEXT NOT NULL,
        conversation_id TEXT NOT NULL,
        PRIMARY KEY (doc_id, conversation_id)
      );
      CREATE INDEX IF NOT EXISTS idx_document_links_conversation ON document_links(conversation_id);
      CREATE TABLE IF NOT EXISTS document_chunks (
        id INTEGER PRIMARY KEY,
        doc_id TEXT NOT NULL,
        ordinal INTEGER NOT NULL,
        page_start INTEGER DEFAULT 0,
        page_end INTEGER DEFAULT 0,
        heading TEXT DEFAULT '',
        text TEXT NOT NULL,
        overlap INTEGER DEFAULT 0
      );
      CREATE INDEX IF NOT EXISTS idx_document_chunks_doc ON document_chunks(doc_id, ordinal);
      CREATE VIRTUAL TABLE IF NOT EXISTS document_chunks_fts USING fts5(
        heading, text, content='document_chunks', content_rowid='id',
        tokenize='porter unicode61 remove_diacritics 2'
      );
      CREATE TRIGGER IF NOT EXISTS document_chunks_ai AFTER INSERT ON document_chunks BEGIN
        INSERT INTO document_chunks_fts(rowid, heading, text) VALUES (new.id, new.heading, new.text);
      END;
      CREATE TRIGGER IF NOT EXISTS document_chunks_ad AFTER DELETE ON document_chunks BEGIN
        INSERT INTO document_chunks_fts(document_chunks_fts, rowid, heading, text)
          VALUES ('delete', old.id, old.heading, old.text);
      END;
    )";
    char* err = nullptr;
    if (sqlite3_exec(db, schema, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[delta-documents] schema init failed: " << (err ? err : "unknown") << std::endl;
        sqlite3_free(err);
        return false;
    }
    // Databases from before digests: add the column; documents get their digest on first use.
    sqlite3_stmt* probe = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT digest FROM documents LIMIT 0", -1, &probe, nullptr) != SQLITE_OK)
        sqlite3_exec(db, "ALTER TABLE documents ADD COLUMN digest TEXT DEFAULT ''", nullptr, nullptr, nullptr);
    sqlite3_finalize(probe);
    db_ = db;
    return true;
}

std::string DocumentStore::add(const std::string& name, const std::string& mime, const std::string& conversation_id,
                               const std::vector<DocumentSegment>& segments, bool library) {
    const auto chunks = chunk_segments(segments);
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    if (!db_ || chunks.empty())
        return "";
    // A document no conversation holds would be unreachable, so keep it for all chats instead.
    if (conversation_id.empty())
        library = true;

    int pages = 0;
    size_t chars = 0;
    for (const auto& s : segments) {
        pages = std::max(pages, s.page);
        chars += s.text.size();
    }

    const std::string id = new_id();
    sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    bool ok = true;

    sqlite3_stmt* st = nullptr;
    ok = sqlite3_prepare_v2(db_,
                            "INSERT INTO documents (id, name, mime, scope, pages, chars, chunks, created_at, digest) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
                            -1, &st, nullptr) == SQLITE_OK;
    if (ok) {
        sqlite3_bind_text(st, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, mime.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, library ? "library" : "conversation", -1, SQLITE_STATIC);
        sqlite3_bind_int(st, 5, pages);
        sqlite3_bind_int64(st, 6, static_cast<sqlite3_int64>(chars));
        sqlite3_bind_int(st, 7, static_cast<int>(chunks.size()));
        sqlite3_bind_text(st, 8, utc_now().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 9, build_digest(chunks, chars).c_str(), -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(st) == SQLITE_DONE;
    }
    sqlite3_finalize(st);

    if (ok && !conversation_id.empty()) {
        st = nullptr;
        ok = sqlite3_prepare_v2(db_, "INSERT OR IGNORE INTO document_links (doc_id, conversation_id) VALUES (?, ?)", -1,
                                &st, nullptr) == SQLITE_OK;
        if (ok) {
            sqlite3_bind_text(st, 1, id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 2, conversation_id.c_str(), -1, SQLITE_TRANSIENT);
            ok = sqlite3_step(st) == SQLITE_DONE;
        }
        sqlite3_finalize(st);
    }

    if (ok) {
        st = nullptr;
        ok = sqlite3_prepare_v2(db_,
                                "INSERT INTO document_chunks (doc_id, ordinal, page_start, page_end, heading, text, "
                                "overlap) VALUES (?, ?, ?, ?, ?, ?, ?)",
                                -1, &st, nullptr) == SQLITE_OK;
        for (size_t i = 0; ok && i < chunks.size(); i++) {
            const auto& c = chunks[i];
            sqlite3_reset(st);
            sqlite3_bind_text(st, 1, id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st, 2, static_cast<int>(i));
            sqlite3_bind_int(st, 3, c.page_start);
            sqlite3_bind_int(st, 4, c.page_end);
            sqlite3_bind_text(st, 5, c.heading.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 6, c.text.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st, 7, c.overlap);
            ok = sqlite3_step(st) == SQLITE_DONE;
        }
        sqlite3_finalize(st);
    }

    if (!ok) {
        std::cerr << "[delta-documents] indexing " << name << " failed: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        return "";
    }
    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
    return id;
}

bool DocumentStore::attach(const std::string& doc_id, const std::string& conversation_id) {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    if (!db_ || conversation_id.empty())
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "INSERT OR IGNORE INTO document_links (doc_id, conversation_id) "
                           "SELECT id, ? FROM documents WHERE id = ?",
                           -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, conversation_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, doc_id.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    if (!ok)
        return false;
    // Already linked also counts; only an unknown document fails.
    DocumentInfo info;
    return resolve(doc_id, conversation_id, info);
}

bool DocumentStore::set_library(const std::string& doc_id, bool library) {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    if (!db_)
        return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE documents SET scope = ? WHERE id = ?", -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, library ? "library" : "conversation", -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, doc_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
    if (sqlite3_changes(db_) == 0)
        return false;
    if (!library) {
        // Taken out of the library: it lives only as long as some conversation still holds it.
        st = nullptr;
        bool linked = false;
        if (sqlite3_prepare_v2(db_, "SELECT 1 FROM document_links WHERE doc_id = ? LIMIT 1", -1, &st, nullptr) ==
            SQLITE_OK) {
            sqlite3_bind_text(st, 1, doc_id.c_str(), -1, SQLITE_TRANSIENT);
            linked = sqlite3_step(st) == SQLITE_ROW;
        }
        sqlite3_finalize(st);
        if (!linked)
            delete_unlocked(doc_id);
    }
    return true;
}

bool DocumentStore::delete_unlocked(const std::string& doc_id) {
    const char* sqls[] = {"DELETE FROM document_chunks WHERE doc_id = ?", "DELETE FROM document_links WHERE doc_id = ?",
                          "DELETE FROM documents WHERE id = ?"};
    int removed = 0;
    for (const char* sql : sqls) {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
            return false;
        sqlite3_bind_text(st, 1, doc_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
        removed = sqlite3_changes(db_);
    }
    return removed > 0;
}

bool DocumentStore::remove(const std::string& doc_id) {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    if (!db_)
        return false;
    sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    bool ok = delete_unlocked(doc_id);
    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
    return ok;
}

int DocumentStore::remove_conversation(const std::string& conversation_id) {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    if (!db_ || conversation_id.empty())
        return 0;
    sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);

    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "DELETE FROM document_links WHERE conversation_id = ?", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, conversation_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);

    // Conversation documents nothing links to any more.
    std::vector<std::string> orphans;
    st = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT id FROM documents WHERE scope = 'conversation' AND id NOT IN "
                           "(SELECT doc_id FROM document_links)",
                           -1, &st, nullptr) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW)
            orphans.push_back(col_text(st, 0));
    }
    sqlite3_finalize(st);
    int removed = 0;
    for (const auto& id : orphans)
        removed += delete_unlocked(id) ? 1 : 0;
    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
    return removed;
}

namespace {

const char* kInfoColumns = "d.id, d.name, d.mime, d.scope, d.pages, d.chars, d.chunks, d.created_at";

DocumentInfo read_info(sqlite3_stmt* st) {
    DocumentInfo d;
    d.id = col_text(st, 0);
    d.name = col_text(st, 1);
    d.mime = col_text(st, 2);
    d.scope = col_text(st, 3);
    d.pages = sqlite3_column_int(st, 4);
    d.chars = sqlite3_column_int(st, 5);
    d.chunks = sqlite3_column_int(st, 6);
    d.created_at = col_text(st, 7);
    return d;
}

// The documents a conversation may read: those attached to it, plus the library when asked.
const char* kAccessible = "(d.id IN (SELECT doc_id FROM document_links WHERE conversation_id = ?1) OR "
                          "(?2 AND d.scope = 'library'))";

} // namespace

std::vector<DocumentInfo> DocumentStore::list(const std::string& conversation_id, bool include_library) const {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    std::vector<DocumentInfo> out;
    if (!db_)
        return out;
    std::string sql = std::string("SELECT ") + kInfoColumns + " FROM documents d WHERE " + kAccessible +
                      " ORDER BY d.created_at, d.name";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        return out;
    sqlite3_bind_text(st, 1, conversation_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, include_library ? 1 : 0);
    while (sqlite3_step(st) == SQLITE_ROW)
        out.push_back(read_info(st));
    sqlite3_finalize(st);
    return out;
}

std::vector<DocumentInfo> DocumentStore::library() const {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    std::vector<DocumentInfo> out;
    if (!db_)
        return out;
    std::string sql =
        std::string("SELECT ") + kInfoColumns + " FROM documents d WHERE d.scope = 'library' ORDER BY d.name";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        return out;
    while (sqlite3_step(st) == SQLITE_ROW)
        out.push_back(read_info(st));
    sqlite3_finalize(st);
    return out;
}

bool DocumentStore::has_documents(const std::string& conversation_id) const {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    if (!db_ || conversation_id.empty())
        return false;
    sqlite3_stmt* st = nullptr;
    bool found = false;
    if (sqlite3_prepare_v2(db_, "SELECT 1 FROM document_links WHERE conversation_id = ? LIMIT 1", -1, &st, nullptr) ==
        SQLITE_OK) {
        sqlite3_bind_text(st, 1, conversation_id.c_str(), -1, SQLITE_TRANSIENT);
        found = sqlite3_step(st) == SQLITE_ROW;
    }
    sqlite3_finalize(st);
    return found;
}

bool DocumentStore::resolve(const std::string& id_or_name, const std::string& conversation_id,
                            DocumentInfo& out) const {
    const auto docs = list(conversation_id, true);
    const std::string want = lower(trim(id_or_name));
    if (want.empty())
        return false;
    for (const auto& d : docs) {
        if (d.id == id_or_name || lower(d.name) == want) {
            out = d;
            return true;
        }
    }
    const DocumentInfo* match = nullptr;
    for (const auto& d : docs) {
        if (lower(d.name).find(want) != std::string::npos) {
            if (match)
                return false; // ambiguous
            match = &d;
        }
    }
    if (!match)
        return false;
    out = *match;
    return true;
}

namespace {

struct ChunkRow {
    int ordinal = 0;
    int page_start = 0;
    int page_end = 0;
    std::string heading;
    std::string text;
    int overlap = 0;
};

// Chunks [from, to] of one document joined into a passage, dropping the text each chunk repeats
// from the one before it.
DocumentExcerpt stitch(const std::string& doc_id, const std::string& doc_name, const std::vector<ChunkRow>& rows) {
    DocumentExcerpt ex;
    ex.doc_id = doc_id;
    ex.doc_name = doc_name;
    for (size_t i = 0; i < rows.size(); i++) {
        const auto& r = rows[i];
        if (i == 0) {
            ex.text = r.text;
            ex.heading = r.heading;
            ex.page_start = r.page_start;
            ex.section_start = r.ordinal + 1;
        } else {
            bool contiguous = r.ordinal == rows[i - 1].ordinal + 1;
            size_t skip = contiguous ? std::min<size_t>(static_cast<size_t>(r.overlap), r.text.size()) : 0;
            ex.text += contiguous ? "\n\n" : "\n\n[...]\n\n";
            ex.text += r.text.substr(skip);
        }
        ex.page_end = std::max(ex.page_end, r.page_end);
        ex.section_end = r.ordinal + 1;
    }
    if (ex.page_start == 0)
        ex.page_end = 0;
    return ex;
}

std::vector<ChunkRow> fetch_chunks(sqlite3* db, const std::string& doc_id, const char* where, int a, int b) {
    std::vector<ChunkRow> rows;
    std::string sql = std::string("SELECT ordinal, page_start, page_end, heading, text, overlap FROM document_chunks "
                                  "WHERE doc_id = ? AND ") +
                      where + " ORDER BY ordinal";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        return rows;
    sqlite3_bind_text(st, 1, doc_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, a);
    sqlite3_bind_int(st, 3, b);
    while (sqlite3_step(st) == SQLITE_ROW) {
        rows.push_back({sqlite3_column_int(st, 0), sqlite3_column_int(st, 1), sqlite3_column_int(st, 2),
                        col_text(st, 3), col_text(st, 4), sqlite3_column_int(st, 5)});
    }
    sqlite3_finalize(st);
    return rows;
}

std::string cut_to(const std::string& text, size_t max_chars) {
    if (text.size() <= max_chars)
        return text;
    size_t cut = utf8_floor(text, max_chars);
    size_t space = text.rfind(' ', cut);
    if (space != std::string::npos && space > max_chars * 3 / 4)
        cut = space;
    return text.substr(0, cut) + " [...]";
}

} // namespace

std::vector<DocumentExcerpt> DocumentStore::search(const std::string& query, const std::string& conversation_id,
                                                   bool include_library, size_t max_chars, int max_hits,
                                                   const std::string& doc_id) const {
    std::vector<DocumentExcerpt> out;
    const std::string match = build_match_query(query);
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    if (!db_ || match.empty() || max_chars == 0 || max_hits <= 0)
        return out;

    // Heading matches count double: a question that names a section should land in it.
    std::string sql = std::string("SELECT c.doc_id, d.name, c.ordinal, length(c.text), "
                                  "bm25(document_chunks_fts, 2.0, 1.0) AS score "
                                  "FROM document_chunks_fts "
                                  "JOIN document_chunks c ON c.id = document_chunks_fts.rowid "
                                  "JOIN documents d ON d.id = c.doc_id "
                                  "WHERE document_chunks_fts MATCH ?3 AND ") +
                      kAccessible + (doc_id.empty() ? "" : " AND d.id = ?5") + " ORDER BY score LIMIT ?4";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
        std::cerr << "[delta-documents] search failed: " << sqlite3_errmsg(db_) << std::endl;
        return out;
    }
    sqlite3_bind_text(st, 1, conversation_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, include_library ? 1 : 0);
    sqlite3_bind_text(st, 3, match.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 4, max_hits);
    if (!doc_id.empty())
        sqlite3_bind_text(st, 5, doc_id.c_str(), -1, SQLITE_TRANSIENT);

    struct Range {
        std::string doc_id, doc_name;
        int from, to;
        int hit; // the chunk that matched best, kept when the budget cannot fit the whole range
        double score;
    };
    std::vector<Range> ranges;
    while (sqlite3_step(st) == SQLITE_ROW) {
        std::string did = col_text(st, 0);
        int ord = sqlite3_column_int(st, 2);
        int len = sqlite3_column_int(st, 3);
        double score = sqlite3_column_double(st, 4);
        // A short hit (a heading, a table row) means little without what surrounds it.
        int pad = len < 900 ? 1 : 0;
        Range r{did, col_text(st, 1), std::max(0, ord - pad), ord + pad, ord, score};
        // Merge with an earlier range of the same document that touches it; the earlier one ranks
        // higher, so it keeps its place and score.
        bool merged = false;
        for (auto& e : ranges) {
            if (e.doc_id == r.doc_id && r.from <= e.to + 1 && r.to >= e.from - 1) {
                e.from = std::min(e.from, r.from);
                e.to = std::max(e.to, r.to);
                merged = true;
                break;
            }
        }
        if (!merged)
            ranges.push_back(r);
    }
    sqlite3_finalize(st);

    size_t used = 0;
    for (const auto& r : ranges) {
        if (used + 200 > max_chars)
            break;
        auto rows = fetch_chunks(db_, r.doc_id, "ordinal BETWEEN ? AND ?", r.from, r.to);
        if (rows.empty())
            continue;
        DocumentExcerpt ex = stitch(r.doc_id, r.doc_name, rows);
        // Too long for what is left: fall back to the matching chunk alone rather than cut the
        // range from its end, which can drop the very sentence that matched.
        if (ex.text.size() > max_chars - used && r.from != r.to) {
            auto hit_rows = fetch_chunks(db_, r.doc_id, "ordinal BETWEEN ? AND ?", r.hit, r.hit);
            if (!hit_rows.empty())
                ex = stitch(r.doc_id, r.doc_name, hit_rows);
        }
        ex.score = r.score;
        ex.text = cut_to(ex.text, max_chars - used);
        used += ex.text.size();
        out.push_back(std::move(ex));
    }
    return out;
}

std::string DocumentStore::digest(const std::string& doc_id) const {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    if (!db_)
        return "";
    std::string digest;
    size_t chars = 0;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT digest, chars FROM documents WHERE id = ?", -1, &st, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, doc_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            digest = col_text(st, 0);
            chars = static_cast<size_t>(sqlite3_column_int64(st, 1));
        }
    }
    sqlite3_finalize(st);
    if (!digest.empty())
        return digest;

    std::vector<DocumentChunk> chunks;
    for (const auto& row : fetch_chunks(db_, doc_id, "ordinal BETWEEN ? AND ?", 0, std::numeric_limits<int>::max()))
        chunks.push_back({row.page_start, row.page_end, row.heading, row.text, row.overlap});
    digest = build_digest(chunks, chars);
    if (!digest.empty() &&
        sqlite3_prepare_v2(db_, "UPDATE documents SET digest = ? WHERE id = ?", -1, &st, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, digest.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, doc_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    return digest;
}

std::vector<DocumentChunk> DocumentStore::sections(const std::string& doc_id) const {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    std::vector<DocumentChunk> out;
    if (!db_)
        return out;
    for (const auto& row : fetch_chunks(db_, doc_id, "ordinal BETWEEN ? AND ?", 0, std::numeric_limits<int>::max())) {
        DocumentChunk chunk;
        chunk.page_start = row.page_start;
        chunk.page_end = row.page_end;
        chunk.heading = row.heading;
        const size_t skip = std::min(row.text.size(), static_cast<size_t>(std::max(0, row.overlap)));
        chunk.text = row.text.substr(skip);
        out.push_back(std::move(chunk));
    }
    return out;
}

DocumentExcerpt DocumentStore::read(const std::string& doc_id, int from, int to, size_t max_chars,
                                    bool& truncated) const {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    truncated = false;
    DocumentExcerpt ex;
    if (!db_)
        return ex;
    sqlite3_stmt* st = nullptr;
    std::string name;
    int pages = 0;
    if (sqlite3_prepare_v2(db_, "SELECT name, pages FROM documents WHERE id = ?", -1, &st, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, doc_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            name = col_text(st, 0);
            pages = sqlite3_column_int(st, 1);
        }
    }
    sqlite3_finalize(st);
    if (name.empty())
        return ex;

    from = std::max(1, from);
    to = std::max(from, to);
    auto rows = pages > 0 ? fetch_chunks(db_, doc_id, "page_end >= ? AND page_start <= ?", from, to)
                          : fetch_chunks(db_, doc_id, "ordinal BETWEEN ? AND ?", from - 1, to - 1);
    ex = stitch(doc_id, name, rows);
    if (ex.text.size() > max_chars) {
        ex.text = cut_to(ex.text, max_chars);
        truncated = true;
    }
    return ex;
}

} // namespace agent
} // namespace delta
