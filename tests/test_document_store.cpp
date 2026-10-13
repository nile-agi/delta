/**
 * Document store tests.
 *
 * Chunking, FTS5 retrieval and conversation scoping against an in-memory SQLite database, so
 * they run in milliseconds and need no model. Run with: ./build/delta-document-tests
 */

#include "agent/document_store.h"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace delta::agent;

static int g_failures = 0;
static int g_checks = 0;
static std::string g_current_test;

static void check(bool condition, const std::string& what) {
    g_checks++;
    if (condition) {
        std::cout << "    ok   " << what << "\n";
    } else {
        g_failures++;
        std::cout << "  FAIL   " << what << "  [" << g_current_test << "]\n";
    }
}

static void section(const std::string& name) {
    g_current_test = name;
    std::cout << "\n" << name << "\n";
}

// A page of plausible filler: several paragraphs, none of which mention the needle.
static std::string filler_page(int page) {
    static const char* topics[] = {"quarterly revenue",    "supply logistics",   "staff training",
                                   "customer onboarding",  "regional expansion", "product roadmap",
                                   "infrastructure costs", "vendor contracts"};
    std::string text;
    for (int p = 0; p < 4; p++) {
        const char* topic = topics[(page * 4 + p) % 8];
        text += "This section reviews " + std::string(topic) +
                " for the period under discussion. The team "
                "compared results against the plan and noted the main differences. Further detail on " +
                topic + " appears in the appendix, with figures for each month of the year.\n\n";
    }
    return text;
}

static sqlite3* open_memory_db() {
    sqlite3* db = nullptr;
    sqlite3_open(":memory:", &db);
    return db;
}

static void test_chunking() {
    section("chunking");
    std::vector<DocumentSegment> pages;
    for (int p = 1; p <= 20; p++)
        pages.push_back({p, "", filler_page(p), false});
    auto chunks = chunk_segments(pages, 1800, 200);
    check(chunks.size() > 5, "a 20-page document becomes several chunks");

    bool sizes_ok = true, pages_ok = true, overlap_ok = true;
    for (size_t i = 0; i < chunks.size(); i++) {
        const auto& c = chunks[i];
        if (c.text.size() > 1800 + 400)
            sizes_ok = false;
        if (c.page_start < 1 || c.page_end < c.page_start || c.page_end > 20)
            pages_ok = false;
        if (i > 0 && c.overlap > 0) {
            // The carried-over text really is the end of the previous chunk.
            std::string carried = c.text.substr(0, static_cast<size_t>(c.overlap));
            if (chunks[i - 1].text.find(carried) == std::string::npos)
                overlap_ok = false;
        }
    }
    check(sizes_ok, "no chunk is much larger than the target");
    check(pages_ok, "every chunk carries a valid page range");
    check(overlap_ok, "overlap repeats the end of the previous chunk");
    check(chunks.front().page_start == 1 && chunks.back().page_end == 20, "page ranges span the whole document");

    // A single paragraph far longer than a chunk is split on sentence boundaries.
    std::string longpara;
    for (int i = 0; i < 80; i++)
        longpara += "Sentence number " + std::to_string(i) + " says something unremarkable. ";
    auto split = chunk_segments({{0, "", longpara, false}}, 1800, 200);
    bool sentence_ends = true;
    for (size_t i = 0; i + 1 < split.size(); i++) {
        const std::string& t = split[i].text;
        if (t.back() != '.')
            sentence_ends = false;
    }
    check(split.size() >= 2, "a very long paragraph is split");
    check(sentence_ends, "long paragraphs are cut at sentence ends");

    // Headings travel with the chunks under them.
    std::string doc = "# Introduction\n\nOpening words about the project.\n\n# Safety Rules\n\n"
                      "Always wear a helmet on the site floor.\n";
    auto headed = chunk_segments({{0, "", doc, false}}, 60, 0);
    bool found_safety = false;
    for (const auto& c : headed)
        if (c.heading == "Safety Rules" && c.text.find("helmet") != std::string::npos)
            found_safety = true;
    check(found_safety, "a chunk knows the heading it sits under");

    // Spreadsheet rows: never split, never overlapped, never mixed across sheets.
    std::string rows;
    for (int r = 0; r < 200; r++)
        rows += "Name: Item " + std::to_string(r) + " | Price: " + std::to_string(r * 3) + " | Stock: 12\n";
    auto sheet =
        chunk_segments({{0, "Inventory", rows, true}, {0, "Staff", "Name: Ada | Role: Engineer\n", true}}, 1800, 200);
    bool rows_whole = true, no_overlap = true, labelled = true;
    for (const auto& c : sheet) {
        if (c.text.find("Name: ") != 0)
            rows_whole = false;
        if (c.overlap != 0)
            no_overlap = false;
        if (c.heading != "Inventory" && c.heading != "Staff")
            labelled = false;
        if (c.heading == "Inventory" && c.text.find("Ada") != std::string::npos)
            labelled = false;
    }
    check(sheet.size() > 2, "a 200-row sheet becomes several chunks");
    check(rows_whole, "every sheet chunk starts at a row");
    check(no_overlap, "sheet chunks carry no overlap");
    check(labelled, "sheet chunks are labelled with their sheet and do not mix sheets");
}

static void test_match_query() {
    section("query building");
    check(build_match_query("What does the contract say about termination?") == "\"contract\" OR \"termination\"",
          "stop words are dropped and terms quoted");
    check(build_match_query("the of and") == "", "a question of only stop words gives no query");
    std::string nasty = build_match_query("\"quoted\" -minus col:umn NEAR(a b) * ^start");
    check(nasty.find('*') == std::string::npos && nasty.find(':') == std::string::npos &&
              nasty.find('(') == std::string::npos && nasty.find('^') == std::string::npos,
          "FTS5 operators in user text never reach the query");
    check(build_match_query("invoice 7 of 2024").find("\"7\"") != std::string::npos, "single digits are kept");
    check(build_match_query("café prices").find("\"café\"") != std::string::npos, "non-ASCII words survive");
}

static void test_store() {
    section("store and retrieval");
    sqlite3* db = open_memory_db();
    auto& store = DocumentStore::instance();
    check(store.init(db), "the store initialises (FTS5 is compiled in)");

    // A 120-page report with one distinctive fact on page 80.
    std::vector<DocumentSegment> pages;
    for (int p = 1; p <= 120; p++) {
        std::string text = filler_page(p);
        if (p == 80)
            text += "The warranty on the hydraulic pump expires after 36 months unless the seals are replaced.\n\n";
        pages.push_back({p, "", text, false});
    }

    auto t0 = std::chrono::steady_clock::now();
    std::string id = store.add("annual-report.pdf", "application/pdf", "conv-a", pages);
    auto t1 = std::chrono::steady_clock::now();
    check(!id.empty(), "a 120-page document is indexed");
    auto index_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    check(index_ms < 1000, "indexing takes under a second (" + std::to_string(index_ms) + " ms)");

    t0 = std::chrono::steady_clock::now();
    auto hits = store.search("When does the hydraulic pump warranty expire?", "conv-a", false, 4000);
    t1 = std::chrono::steady_clock::now();
    auto search_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    check(!hits.empty(), "the question finds something");
    check(!hits.empty() && hits[0].text.find("36 months") != std::string::npos, "the best excerpt holds the answer");
    check(!hits.empty() && hits[0].page_start <= 80 && hits[0].page_end >= 80, "and cites page 80");
    check(!hits.empty() && hits[0].citation().find("annual-report.pdf, p.") == 0, "citations name the file and page");
    check(search_us < 10000, "search takes under 10 ms (" + std::to_string(search_us) + " us)");
    {
        const auto j = hits.empty() ? nlohmann::json() : hits[0].to_json();
        check(j.value("page_start", 0) <= 80 && j.value("page_end", 0) >= 80 && j.value("document_id", "") == id,
              "the JSON carries the page numbers, for opening the file there");
    }

    // The whole text in order, for the viewer: overlap removed, every page present once.
    {
        auto sections = store.sections(id);
        std::string all;
        for (const auto& c : sections)
            all += c.text;
        bool ordered = !sections.empty();
        for (size_t i = 1; i < sections.size(); i++)
            ordered = ordered && sections[i].page_start >= sections[i - 1].page_start;
        check(ordered, "sections() returns the chunks in page order");
        size_t n80 = 0;
        for (size_t at = all.find("hydraulic pump"); at != std::string::npos; at = all.find("hydraulic pump", at + 1))
            n80++;
        check(n80 == 1, "with the overlap removed, so no sentence appears twice");
        check(all.find(filler_page(1).substr(0, 40)) != std::string::npos, "starting from the first page");
    }

    size_t total = 0;
    for (auto& h : store.search("revenue logistics training roadmap", "conv-a", false, 3000))
        total += h.text.size();
    check(total <= 3000 + 10, "excerpts respect the character budget");

    // Scoping.
    check(store.search("hydraulic pump", "conv-b", false, 4000).empty(), "another conversation sees nothing");
    check(store.has_documents("conv-a") && !store.has_documents("conv-b"), "has_documents follows the links");

    // Reading by page.
    bool truncated = false;
    auto page80 = store.read(id, 80, 80, 6000, truncated);
    check(page80.text.find("hydraulic pump") != std::string::npos, "read() returns the requested page");
    auto big = store.read(id, 1, 120, 2000, truncated);
    check(truncated && big.text.size() <= 2010, "read() caps a large range and says so");

    // Resolving by name.
    DocumentInfo info;
    check(store.resolve("annual", "conv-a", info) && info.id == id, "a document resolves by part of its name");
    check(!store.resolve("annual", "conv-b", info), "but not from a conversation that lacks it");

    // The library: kept documents reach every conversation, but only when asked.
    check(store.set_library(id, true), "a document can be kept for all chats");
    check(store.search("hydraulic pump", "conv-b", false, 4000).empty(), "library is not searched unless asked");
    check(!store.search("hydraulic pump", "conv-b", true, 4000).empty(), "library is searchable from any chat");

    // Deleting a conversation keeps library documents and removes the rest.
    std::string notes =
        store.add("notes.md", "text/markdown", "conv-a", {{0, "", "Zebracorn inventory notes.\n", false}});
    check(!notes.empty(), "a second document is indexed");
    check(store.remove_conversation("conv-a") == 1, "deleting a chat removes its unkept documents");
    check(store.list("", true).size() == 1, "the kept document survives");
    check(store.search("zebracorn", "conv-a", true, 4000).empty(), "and the removed one's index is gone");

    // Unkeeping a document no chat holds deletes it.
    check(store.set_library(id, false) && store.list("", true).empty(), "unkeeping an unlinked document deletes it");

    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db, "SELECT count(*) FROM document_chunks", -1, &st, nullptr);
    sqlite3_step(st);
    check(sqlite3_column_int(st, 0) == 0, "no chunks are left behind");
    sqlite3_finalize(st);

    store.detach();
    sqlite3_close(db);
}

int main() {
    test_chunking();
    test_match_query();
    test_store();
    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
