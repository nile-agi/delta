#include "text_tool_call.h"
#include <cctype>

namespace delta {
namespace agent {

nlohmann::json parse_kwargs(const std::string& s) {
    nlohmann::json out = nlohmann::json::object();
    size_t i = 0;
    auto skip_ws = [&]() {
        while (i < s.size() && std::isspace((unsigned char)s[i]))
            i++;
    };
    while (i < s.size()) {
        skip_ws();
        size_t ks = i;
        while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_'))
            i++;
        if (i == ks)
            break;
        std::string key = s.substr(ks, i - ks);
        skip_ws();
        if (i >= s.size() || (s[i] != '=' && s[i] != ':'))
            break;
        i++;
        skip_ws();
        if (i < s.size() && (s[i] == '"' || s[i] == '\'')) {
            char q = s[i++];
            std::string val;
            while (i < s.size() && s[i] != q) {
                if (s[i] == '\\' && i + 1 < s.size()) {
                    val += s[i + 1];
                    i += 2;
                } else {
                    val += s[i++];
                }
            }
            if (i < s.size())
                i++;
            out[key] = val;
        } else if (i < s.size() && (s[i] == '[' || s[i] == '{')) {
            const char open = s[i];
            const char close = open == '[' ? ']' : '}';
            int depth = 0;
            size_t vs = i;
            while (i < s.size()) {
                if (s[i] == open) {
                    depth++;
                } else if (s[i] == close) {
                    if (--depth == 0) {
                        i++;
                        break;
                    }
                } else if (s[i] == '"') {
                    i++;
                    while (i < s.size() && s[i] != '"')
                        i++;
                }
                i++;
            }
            const std::string rawv = s.substr(vs, i - vs);
            try {
                out[key] = nlohmann::json::parse(rawv);
            } catch (...) {
                out[key] = rawv;
            }
        } else {
            size_t vs = i;
            while (i < s.size() && s[i] != ',')
                i++;
            std::string rawv = s.substr(vs, i - vs);
            while (!rawv.empty() && std::isspace((unsigned char)rawv.back()))
                rawv.pop_back();
            if (rawv == "true" || rawv == "True") {
                out[key] = true;
            } else if (rawv == "false" || rawv == "False") {
                out[key] = false;
            } else {
                try {
                    size_t used = 0;
                    const double d = std::stod(rawv, &used);
                    if (used == rawv.size())
                        out[key] = d == static_cast<long long>(d) ? nlohmann::json(static_cast<long long>(d))
                                                                  : nlohmann::json(d);
                    else
                        out[key] = rawv;
                } catch (...) {
                    out[key] = rawv;
                }
            }
        }
        skip_ws();
        if (i < s.size() && s[i] == ',')
            i++;
    }
    return out;
}

bool recover_text_tool_call(const std::string& content,
                            const std::function<std::string(const std::string&, nlohmann::json&)>& resolve,
                            std::string& tool_name, nlohmann::json& arguments) {
    for (size_t pos = content.find('('); pos != std::string::npos; pos = content.find('(', pos + 1)) {
        size_t start = pos;
        while (start > 0 && (std::isalnum((unsigned char)content[start - 1]) || content[start - 1] == '_'))
            start--;
        if (start == pos)
            continue;
        const std::string written = content.substr(start, pos - start);

        // The matching close paren, skipping any inside quoted strings.
        size_t i = pos + 1;
        int depth = 1;
        while (i < content.size() && depth > 0) {
            const char ch = content[i];
            if (ch == '(') {
                depth++;
            } else if (ch == ')') {
                depth--;
            } else if (ch == '"' || ch == '\'') {
                i++;
                while (i < content.size() && content[i] != ch)
                    i++;
            }
            i++;
        }
        if (depth != 0)
            continue;
        std::string inner = content.substr(pos + 1, (i - 1) - (pos + 1));

        nlohmann::json args = nlohmann::json::object();
        size_t first = inner.find_first_not_of(" \t\r\n");
        if (first != std::string::npos && inner[first] == '{') {
            try {
                args = nlohmann::json::parse(inner.substr(first));
            } catch (...) {
                args = nlohmann::json::object();
            }
            if (!args.is_object())
                args = nlohmann::json::object();
        } else {
            args = parse_kwargs(inner);
        }

        const std::string resolved = resolve(written, args);
        if (resolved.empty())
            continue;
        tool_name = resolved;
        arguments = args;
        return true;
    }
    return false;
}

} // namespace agent
} // namespace delta
