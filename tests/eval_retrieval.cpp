/**
 * Retrieval eval for document search.
 *
 * Indexes the frozen corpus in tests/data/retrieval/corpus into one conversation, as if the user
 * had attached all of it to a chat, then asks every question in questions.json and checks whether
 * the passage holding the answer comes back, and how high. No model is involved: this measures
 * the part of RAG the engine controls.
 *
 *   cmake --build build --target delta-retrieval-eval
 *   ./build/delta-retrieval-eval [tests/data/retrieval] [--verbose]
 *
 * Reports hit@1/3/5 and MRR over the ranked passages, whether the answer survives the passage
 * budget the harness actually uses at 4K, 8K and 32K context, how much smaller the injected text
 * is than the files, and search latency.
 */

#include "agent/document_store.h"
#include "json.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace delta::agent;
using json = nlohmann::json;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Markdown has no pages; cut it into ~3,000-character "pages" at blank lines so it is indexed
// the way a PDF of the same text would be, citations included.
std::vector<DocumentSegment> as_pages(const std::string& text) {
    std::vector<DocumentSegment> pages;
    std::string page;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find("\n\n", start);
        end = end == std::string::npos ? text.size() : end + 2;
        page += text.substr(start, end - start);
        start = end;
        if (page.size() >= 3000) {
            pages.push_back({static_cast<int>(pages.size()) + 1, "", page, false});
            page.clear();
        }
    }
    if (!page.empty())
        pages.push_back({static_cast<int>(pages.size()) + 1, "", page, false});
    return pages;
}

bool holds_answer(const DocumentExcerpt& ex, const json& q) {
    if (ex.doc_name != q["doc"].get<std::string>())
        return false;
    for (const auto& a : q["answer"])
        if (ex.text.find(a.get<std::string>()) != std::string::npos)
            return true;
    return false;
}

// The harness's passage budget for a context window (harness.cpp, Harness::run).
size_t budget_for(int n_ctx) {
    return std::min<size_t>(24000, static_cast<size_t>(n_ctx) * 3 / 4);
}

double percentile(std::vector<double> v, double p) {
    if (v.empty())
        return 0;
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, static_cast<size_t>(p * (v.size() - 1) + 0.5))];
}

struct Tally {
    int n = 0, hit1 = 0, hit3 = 0, hit5 = 0;
    double rr = 0;
    std::map<int, int> in_budget; // n_ctx -> answered within that budget
};

} // namespace

int main(int argc, char** argv) {
    std::string dir = "tests/data/retrieval";
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--verbose")
            verbose = true;
        else
            dir = a;
    }

    json spec;
    try {
        spec = json::parse(read_file(dir + "/questions.json"));
    } catch (const std::exception& e) {
        std::cerr << "cannot read " << dir << "/questions.json: " << e.what() << "\n";
        return 2;
    }

    sqlite3* db = nullptr;
    sqlite3_open(":memory:", &db);
    auto& store = DocumentStore::instance();
    if (!store.init(db)) {
        std::cerr << "document store failed to initialise (FTS5 missing?)\n";
        return 2;
    }

    // Every file in the corpus is attached, including those no question is about: they are the
    // distractors a real chat would have.
    const std::string cid = "eval";
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(dir + "/corpus"))
        if (entry.path().extension() == ".md")
            names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    size_t corpus_chars = 0;
    double index_ms = 0;
    for (const auto& name : names) {
        const std::string text = read_file(dir + "/corpus/" + name);
        corpus_chars += text.size();
        auto t0 = std::chrono::steady_clock::now();
        if (store.add(name, "text/markdown", cid, as_pages(text)).empty()) {
            std::cerr << "failed to index " << name << "\n";
            return 2;
        }
        index_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
    int chunks = 0;
    for (const auto& d : store.list(cid, false))
        chunks += d.chunks;

    const std::vector<int> windows = {4096, 8192, 32768};
    std::map<std::string, Tally> by_style;
    Tally all;
    std::vector<double> latencies_ms;
    double injected_chars_8k = 0;

    std::printf("\n%-4s %-10s %-5s %-6s %-6s %-6s  %s\n", "#", "style", "rank", "4K", "8K", "32K", "question");
    int index = 0;
    for (const auto& q : spec["questions"]) {
        index++;
        const std::string question = q["q"];
        const std::string style = q.value("style", "keyword");

        // Ranking: generous budget, so every hit is returned in order.
        auto ranked = store.search(question, cid, false, 1000000, 8);
        int rank = 0;
        for (size_t i = 0; i < ranked.size(); i++) {
            if (holds_answer(ranked[i], q)) {
                rank = static_cast<int>(i) + 1;
                break;
            }
        }

        std::map<int, bool> found;
        for (int n_ctx : windows) {
            auto t0 = std::chrono::steady_clock::now();
            auto passages = store.search(question, cid, false, budget_for(n_ctx));
            latencies_ms.push_back(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
            found[n_ctx] = std::any_of(passages.begin(), passages.end(),
                                       [&](const DocumentExcerpt& ex) { return holds_answer(ex, q); });
            if (n_ctx == 8192)
                for (const auto& p : passages)
                    injected_chars_8k += p.text.size();
        }

        for (Tally* t : {&all, &by_style[style]}) {
            t->n++;
            t->hit1 += rank == 1;
            t->hit3 += rank >= 1 && rank <= 3;
            t->hit5 += rank >= 1 && rank <= 5;
            t->rr += rank ? 1.0 / rank : 0;
            for (int n_ctx : windows)
                t->in_budget[n_ctx] += found[n_ctx];
        }

        std::printf("%-4d %-10s %-5s %-6s %-6s %-6s  %s\n", index, style.c_str(),
                    rank ? std::to_string(rank).c_str() : "-", found[4096] ? "yes" : "NO", found[8192] ? "yes" : "NO",
                    found[32768] ? "yes" : "NO", question.c_str());
        if (verbose && !ranked.empty() && rank != 1)
            std::printf("       top: [%s] %s\n", ranked[0].citation().c_str(), ranked[0].text.substr(0, 140).c_str());
    }

    auto pct = [](int a, int n) { return n ? 100.0 * a / n : 0.0; };
    auto line = [&](const std::string& label, const Tally& t) {
        std::printf("%-11s %3d  hit@1 %5.1f%%  hit@3 %5.1f%%  hit@5 %5.1f%%  MRR %.3f  in budget 4K %5.1f%%  8K "
                    "%5.1f%%  32K %5.1f%%\n",
                    label.c_str(), t.n, pct(t.hit1, t.n), pct(t.hit3, t.n), pct(t.hit5, t.n), t.n ? t.rr / t.n : 0,
                    pct(t.in_budget.at(4096), t.n), pct(t.in_budget.at(8192), t.n), pct(t.in_budget.at(32768), t.n));
    };

    std::printf("\nCorpus: %zu files, %zu chars, %d chunks, indexed in %.1f ms\n", names.size(), corpus_chars, chunks,
                index_ms);
    std::printf("\n");
    line("all", all);
    for (const auto& [style, t] : by_style)
        line(style, t);
    std::printf("\nContext at 8K: %.0f chars of passages per question on average, %.1f%% of the attached text\n",
                injected_chars_8k / all.n, 100.0 * injected_chars_8k / all.n / corpus_chars);
    std::printf("Search latency: p50 %.2f ms, p95 %.2f ms\n", percentile(latencies_ms, 0.5),
                percentile(latencies_ms, 0.95));

    store.detach();
    sqlite3_close(db);
    return 0;
}
