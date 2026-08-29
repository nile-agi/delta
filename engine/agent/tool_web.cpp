#include "tool_web.h"
#include "context_manager.h"
#include "tool_registry.h"
#include <algorithm>
#include <cctype>
#include <curl/curl.h>

namespace delta {
namespace agent {

namespace {

constexpr size_t kMaxFetchBytes = 400000; // hard stop on the download itself
constexpr size_t kMaxTextChars = 20000;   // what actually reaches the model

size_t collect(void* contents, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    const size_t total = size * nmemb;
    if (out->size() + total > kMaxFetchBytes)
        return 0; // aborts the transfer
    out->append(static_cast<char*>(contents), total);
    return total;
}

// Strips tags, script and style bodies, and collapses whitespace. Not a real HTML parser -- just
// enough to turn a page into something a model can read.
std::string html_to_text(const std::string& html) {
    std::string out;
    out.reserve(html.size() / 2);
    bool in_tag = false;
    size_t i = 0;
    while (i < html.size()) {
        if (!in_tag && html.compare(i, 7, "<script") == 0) {
            const size_t end = html.find("</script", i);
            i = (end == std::string::npos) ? html.size() : end + 8;
            continue;
        }
        if (!in_tag && html.compare(i, 6, "<style") == 0) {
            const size_t end = html.find("</style", i);
            i = (end == std::string::npos) ? html.size() : end + 7;
            continue;
        }
        const char c = html[i];
        if (c == '<') {
            in_tag = true;
        } else if (c == '>') {
            in_tag = false;
            if (!out.empty() && out.back() != ' ' && out.back() != '\n')
                out += ' ';
        } else if (!in_tag) {
            out += c;
        }
        i++;
    }

    // Decode the handful of entities that matter for readability.
    static const std::pair<const char*, const char*> entities[] = {{"&nbsp;", " "}, {"&amp;", "&"},   {"&lt;", "<"},
                                                                   {"&gt;", ">"},   {"&quot;", "\""}, {"&#39;", "'"}};
    for (const auto& [from, to] : entities) {
        size_t pos = 0;
        const size_t from_len = std::char_traits<char>::length(from);
        while ((pos = out.find(from, pos)) != std::string::npos) {
            out.replace(pos, from_len, to);
            pos += std::char_traits<char>::length(to);
        }
    }

    // Collapse runs of whitespace, keeping paragraph breaks.
    std::string collapsed;
    collapsed.reserve(out.size());
    int newlines = 0;
    bool pending_space = false;
    for (char c : out) {
        if (c == '\n' || c == '\r') {
            newlines++;
            pending_space = false;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            pending_space = true;
            continue;
        }
        if (newlines > 0) {
            collapsed += (newlines > 1 ? "\n\n" : "\n");
            newlines = 0;
            pending_space = false;
        } else if (pending_space && !collapsed.empty()) {
            collapsed += ' ';
        }
        pending_space = false;
        collapsed += c;
    }
    return collapsed;
}

} // namespace

void register_web_tools() {
    auto& registry = ToolRegistry::instance();

    registry.register_tool(
        {"fetch_url",
         "Fetch a web page or API response over HTTP and read it as text. Delta is otherwise "
         "offline, so this is the only tool that leaves the machine -- use it when the user asks "
         "you to look something up at a specific address, and say what you are fetching.",
         {{"type", "object"},
          {"properties",
           {{"url", {{"type", "string"}, {"description", "Full http:// or https:// URL"}}},
            {"raw",
             {{"type", "boolean"},
              {"description", "Return the body unprocessed instead of stripping HTML (default false)"}}}}},
          {"required", {"url"}}},
         ToolRisk::Caution,
         "web"},
        [](const nlohmann::json& args) -> ToolResult {
            const std::string url = args.value("url", "");
            if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
                return {false, "", "url must start with http:// or https://"};

            CURL* curl = curl_easy_init();
            if (!curl)
                return {false, "", "Could not initialise the HTTP client"};

            std::string body;
            char errbuf[CURL_ERROR_SIZE] = {0};
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Delta/1.0");
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
            curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

            const CURLcode res = curl_easy_perform(curl);
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            char* content_type = nullptr;
            curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
            const std::string ctype = content_type ? content_type : "";
            curl_easy_cleanup(curl);

            // A write-callback abort means the size cap tripped; the body we have is still usable.
            if (res != CURLE_OK && !(res == CURLE_WRITE_ERROR && !body.empty()))
                return {false, "", std::string("Fetch failed: ") + (errbuf[0] ? errbuf : curl_easy_strerror(res))};

            const bool looks_html = ctype.find("html") != std::string::npos ||
                                    body.find("<html") != std::string::npos ||
                                    body.find("<!DOCTYPE") != std::string::npos;
            std::string text = (args.value("raw", false) || !looks_html) ? body : html_to_text(body);

            const bool truncated = text.size() > kMaxTextChars;
            if (truncated)
                text = ContextManager::truncate_middle(text, kMaxTextChars);

            return {true,
                    nlohmann::json{{"url", url},
                                   {"status", http_code},
                                   {"content_type", ctype},
                                   {"truncated", truncated},
                                   {"content", text}}
                        .dump(),
                    ""};
        });

    registry.register_tool(
        {"open_in_browser",
         "Open a URL in the user's default web browser. Use it when the user should look at a page "
         "themselves; use fetch_url when you need to read the contents.",
         {{"type", "object"},
          {"properties", {{"url", {{"type", "string"}, {"description", "Full http:// or https:// URL"}}}}},
          {"required", {"url"}}},
         ToolRisk::Caution,
         "web"},
        [](const nlohmann::json& args) -> ToolResult {
            const std::string url = args.value("url", "");
            if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
                return {false, "", "url must start with http:// or https://"};
            // Quoting matters here: the URL goes to a shell.
            if (url.find('"') != std::string::npos || url.find('`') != std::string::npos ||
                url.find('$') != std::string::npos)
                return {false, "", "That URL contains characters Delta will not pass to the shell"};
#if defined(_WIN32)
            const std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
            const std::string cmd = "open \"" + url + "\"";
#else
            const std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
#endif
            const int rc = std::system(cmd.c_str());
            if (rc != 0)
                return {false, "", "Could not open the browser"};
            return {true, nlohmann::json{{"opened", true}, {"url", url}}.dump(), ""};
        });
}

} // namespace agent
} // namespace delta
