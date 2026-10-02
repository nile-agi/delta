#include "quick_add.h"
#include "time_compat.h"
#include "tool_calendar.h"
#include <algorithm>
#include <cctype>
#include <regex>

namespace delta {
namespace agent {

namespace {

const auto kIcase = std::regex::ECMAScript | std::regex::icase;

const char* const kWeekdays[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
const char* const kMonths[] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};

const std::string kWeekdayAlt = "(?:sun|mon|tues?|wed(?:nes)?|thu(?:rs?)?|fri|sat(?:ur)?)(?:day)?";
const std::string kMonthAlt =
    "(?:jan(?:uary)?|feb(?:ruary)?|mar(?:ch)?|apr(?:il)?|may|june?|july?|aug(?:ust)?|sep(?:t(?:ember)?)?|oct(?:ober)?|"
    "nov(?:ember)?|dec(?:ember)?)";

std::string lower_of(const std::string& s) {
    std::string out = s;
    for (auto& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool matches(const std::string& text, const std::string& pattern) {
    return std::regex_search(text, std::regex(pattern, kIcase));
}

std::string erase_all(const std::string& text, const std::string& pattern) {
    return std::regex_replace(text, std::regex(pattern, kIcase), " ");
}

std::tm day_offset(std::time_t now, int days) {
    // Noon, so a DST change can never push the result onto a neighbouring day.
    std::tm t{};
    local_time(&now, &t);
    t.tm_hour = 12;
    t.tm_min = 0;
    t.tm_sec = 0;
    t.tm_mday += days;
    t.tm_isdst = -1;
    std::mktime(&t);
    return t;
}

int month_index(const std::string& word) {
    const std::string w = lower_of(word).substr(0, 3);
    for (int m = 0; m < 12; m++)
        if (w == kMonths[m])
            return m;
    return -1;
}

// The date the text names, if any.
std::optional<std::tm> find_day(const std::string& lower, std::time_t now) {
    std::tm today{};
    local_time(&now, &today);
    std::smatch m;

    if (matches(lower, "\\bday after tomorrow\\b"))
        return day_offset(now, 2);
    if (matches(lower, "\\btomorrow\\b"))
        return day_offset(now, 1);
    if (matches(lower, "\\b(today|tonight|this (morning|afternoon|evening))\\b"))
        return day_offset(now, 0);

    if (std::regex_search(lower, m, std::regex("\\b(\\d{4})-(\\d{2})-(\\d{2})\\b"))) {
        std::tm t{};
        t.tm_year = std::stoi(m[1]) - 1900;
        t.tm_mon = std::stoi(m[2]) - 1;
        t.tm_mday = std::stoi(m[3]);
        t.tm_hour = 12;
        t.tm_isdst = -1;
        std::mktime(&t);
        return t;
    }

    // "3 october", "3rd of oct", "october 3", "oct 3rd"
    int day = 0, month = -1;
    if (std::regex_search(lower, m, std::regex("\\b(\\d{1,2})(?:st|nd|rd|th)?\\s+(?:of\\s+)?(" + kMonthAlt + ")\\b"))) {
        day = std::stoi(m[1]);
        month = month_index(m[2]);
    } else if (std::regex_search(lower, m, std::regex("\\b(" + kMonthAlt + ")\\s+(\\d{1,2})(?:st|nd|rd|th)?\\b"))) {
        month = month_index(m[1]);
        day = std::stoi(m[2]);
    }
    if (month >= 0 && day >= 1 && day <= 31) {
        std::tm t{};
        t.tm_year = today.tm_year;
        t.tm_mon = month;
        t.tm_mday = day;
        t.tm_hour = 12;
        t.tm_isdst = -1;
        std::mktime(&t);
        // A date already behind us this year means next year's.
        if (t.tm_mon < today.tm_mon || (t.tm_mon == today.tm_mon && t.tm_mday < today.tm_mday)) {
            t.tm_year++;
            t.tm_isdst = -1;
            std::mktime(&t);
        }
        return t;
    }

    for (int d = 0; d < 7; d++) {
        const std::string name = kWeekdays[d];
        if (!matches(lower, "\\b" + name.substr(0, 3) + "(" + name.substr(3) + ")?\\b"))
            continue;
        int ahead = d - today.tm_wday;
        if (ahead <= 0)
            ahead += 7; // "on Saturday", said on a Saturday, means next week's
        return day_offset(now, ahead);
    }
    return std::nullopt;
}

// The clock time the text names, or nothing. Only trusts a number that reads as a time.
std::optional<std::string> find_time(const std::string& lower) {
    const bool cued = matches(lower, "\\b(noon|midday|midnight)\\b") || matches(lower, "\\b\\d{1,2}:\\d{2}\\b") ||
                      matches(lower, "\\b\\d{1,2}\\s*(am|pm)\\b") ||
                      matches(lower, "\\b(at|around|about|by|from)\\s+(around\\s+|about\\s+)?\\d{1,4}\\b");
    if (cued) {
        if (auto t = extract_clock_time(lower))
            return t;
        // "at 3": a lone digit. Nobody books 3 in the morning like that.
        std::smatch m;
        if (std::regex_search(lower, m,
                              std::regex("\\b(?:at|around|about|by)\\s+(?:around\\s+|about\\s+)?(\\d{1,2})\\b"))) {
            int h = std::stoi(m[1]);
            if (h >= 1 && h <= 12) {
                if (h < 8)
                    h += 12;
                char buf[6];
                std::snprintf(buf, sizeof(buf), "%02d:00", h);
                return std::string(buf);
            }
        }
    }
    if (matches(lower, "\\bthis morning\\b"))
        return std::string("09:00");
    if (matches(lower, "\\b(this )?afternoon\\b"))
        return std::string("14:00");
    if (matches(lower, "\\b(tonight|this evening|evening)\\b"))
        return std::string("19:00");
    return std::nullopt;
}

std::string clean_title(std::string title) {
    const std::string lead = "(?:at|around|about|by|from|@)\\s+(?:around\\s+|about\\s+)?";
    // Times
    title = erase_all(title, "\\b(?:" + lead + ")?(?:noon|midday|midnight)(?:\\s+\\d{3,5})?\\b");
    title = erase_all(title, "\\b(?:" + lead + ")?\\d{1,2}(?::\\d{2})?\\s*(?:am|pm)\\b");
    title = erase_all(title, "\\b(?:" + lead + ")?\\d{1,2}:\\d{2}\\b");
    title = erase_all(title, "\\b" + lead + "\\d{1,4}\\b");
    title = erase_all(title, "\\b(?:this|in the)\\s+(?:morning|afternoon|evening)\\b");
    // Days
    title = erase_all(title, "\\b(?:the\\s+)?day after tomorrow\\b");
    title = erase_all(title, "\\b(?:today|tonight|tomorrow)\\b");
    title = erase_all(title, "\\b(?:on\\s+|next\\s+|this\\s+|coming\\s+)?" + kWeekdayAlt + "s?\\b");
    title = erase_all(title, "\\b(?:on\\s+)?\\d{4}-\\d{2}-\\d{2}\\b");
    title = erase_all(title, "\\b(?:on\\s+)?(?:the\\s+)?\\d{1,2}(?:st|nd|rd|th)?\\s+(?:of\\s+)?" + kMonthAlt + "\\b");
    title = erase_all(title, "\\b(?:on\\s+)?" + kMonthAlt + "\\s+\\d{1,2}(?:st|nd|rd|th)?\\b");
    title = std::regex_replace(title, std::regex("\\s+"), " ");

    // Lead-in words, repeatedly: "so today I will be attending the ..." has several layers.
    static const std::regex lead_ins[] = {
        std::regex("^\\s*(?:so|ok|okay|hey|hi|please|just|also|and|then|well|oh)\\b[,\\s]*", kIcase),
        std::regex("^\\s*i\\s*(?:will|'ll|am|'m|have|'ve|need|must|should|want|got|shall)\\b"
                   "(?:\\s+(?:be|to|got|going|gonna|have))*\\s*",
                   kIcase),
        std::regex("^\\s*(?:remind me(?:\\s+to)?|don'?t forget(?:\\s+to)?|add|schedule|book|create|set up|put|plan)\\b"
                   "\\s*",
                   kIcase),
        std::regex("^\\s*(?:attending|attend|going to|go to|gonna|joining|join|heading to)\\b\\s*", kIcase),
        std::regex("^\\s*(?:an?|the|my)\\s+", kIcase),
    };
    for (int pass = 0; pass < 6; pass++) {
        const std::string before = title;
        for (const auto& re : lead_ins)
            title = std::regex_replace(title, re, "");
        if (title == before)
            break;
    }

    static const std::regex trailing("(?:[\\s,.;:!?-]+|\\s+(?:at|on|around|about|by|for|in|from|to|with|and|please))$",
                                     kIcase);
    for (int pass = 0; pass < 6; pass++) {
        const std::string before = title;
        title = std::regex_replace(title, trailing, "");
        if (title == before)
            break;
    }
    while (!title.empty() && std::isspace(static_cast<unsigned char>(title.front())))
        title.erase(0, 1);

    if (title.size() > 80) {
        const size_t cut = title.rfind(' ', 80);
        title = title.substr(0, cut == std::string::npos ? 80 : cut);
    }
    if (!title.empty())
        title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
    return title;
}

const std::string kMoveVerbs =
    "\\b(move|moving|reschedule|push|postpone|shift|delay|put off|bring forward|change the (date|time))\\b";

// What is left after cleaning must read like a name. "I think i had a task ... move it to" does not,
// and a chip carrying it is worse than no chip.
bool plausible_title(const std::string& title) {
    if (title.size() < 2)
        return false;
    size_t words = 1;
    for (char c : title)
        if (c == ' ')
            words++;
    if (words > 8)
        return false;
    return !matches(lower_of(title), "\\b(i|it|think|want|wanna|maybe|something)\\b");
}

// "YYYY-MM-DDTHH:MM[:SS]" into a local tm, or nothing.
std::optional<std::tm> parse_stamp(const std::string& stamp) {
    std::smatch m;
    if (!std::regex_search(stamp, m, std::regex("^(\\d{4})-(\\d{2})-(\\d{2})(?:[T ](\\d{2}):(\\d{2}))?")))
        return std::nullopt;
    std::tm t{};
    t.tm_year = std::stoi(m[1]) - 1900;
    t.tm_mon = std::stoi(m[2]) - 1;
    t.tm_mday = std::stoi(m[3]);
    t.tm_hour = m[4].matched ? std::stoi(m[4]) : 0;
    t.tm_min = m[5].matched ? std::stoi(m[5]) : 0;
    t.tm_isdst = -1;
    std::mktime(&t);
    return t;
}

std::string format_stamp(const std::tm& t) {
    char buf[24];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:00", &t);
    return buf;
}

std::string date_of(const std::tm& t) {
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &t);
    return buf;
}

// Words that could name an item: not filler, not the request's own verbs.
std::vector<std::string> content_words(const std::string& lower) {
    static const std::regex word("[a-z0-9]{3,}");
    static const std::string stop =
        "|the|and|for|with|that|this|think|had|have|has|was|want|wanted|need|task|tasks|event|events|meeting|"
        "item|one|thing|current|currently|today|tomorrow|yesterday|week|day|move|moving|reschedule|push|postpone|"
        "shift|delay|put|off|bring|forward|change|date|time|same|from|into|onto|please|can|could|would|you|your|"
        "just|also|then|there|here|calendar|schedule|about|around|monday|tuesday|wednesday|thursday|friday|"
        "saturday|sunday|morning|afternoon|evening|tonight|next|coming|";
    std::vector<std::string> out;
    for (auto it = std::sregex_iterator(lower.begin(), lower.end(), word); it != std::sregex_iterator(); ++it) {
        const std::string w = it->str();
        if (stop.find("|" + w + "|") == std::string::npos)
            out.push_back(w);
    }
    return out;
}

} // namespace

nlohmann::json QuickMoveOption::to_json() const {
    nlohmann::json j = {{"id", id},
                        {"title", title},
                        {"type", type},
                        {"start_time", start_time},
                        {"new_start_time", new_start_time},
                        {"all_day", all_day}};
    if (!new_end_time.empty())
        j["new_end_time"] = new_end_time;
    return j;
}

std::vector<QuickMoveOption> parse_quick_move(const std::string& text, std::time_t now,
                                              const std::vector<nlohmann::json>& items) {
    std::vector<QuickMoveOption> out;
    if (text.empty() || text.size() > 300)
        return out;
    const std::string lower = lower_of(text);
    std::smatch verb;
    if (!std::regex_search(lower, verb, std::regex(kMoveVerbs, kIcase)))
        return out;

    // Up to "to"/"until" says which item ("the task I had today", "push the standup"); after it,
    // where it goes.
    std::string source = verb.prefix().str();
    std::string target = verb.suffix().str();
    std::smatch to;
    if (std::regex_search(target, to, std::regex("\\b(to|until|till|for)\\b"))) {
        source += " " + to.prefix().str();
        target = to.suffix().str();
    }
    const auto target_day = find_day(target, now);
    const bool same_time = matches(target, "\\bsame time\\b");
    const auto target_time = same_time ? std::nullopt : find_time(target);
    if (!target_day && !target_time)
        return out;

    std::optional<std::tm> source_day = find_day(source, now);
    if (!source_day && matches(source, "\\bcurrent(ly)?\\b"))
        source_day = day_offset(now, 0);
    const std::string source_date = source_day ? date_of(*source_day) : std::string();

    std::string type_hint;
    if (matches(source, "\\b(task|todo|to-do|reminder)s?\\b"))
        type_hint = "task";
    else if (matches(source, "\\b(meeting|event|appointment|call)s?\\b"))
        type_hint = "event";
    const auto words = content_words(source);
    const std::string today = date_of(day_offset(now, 0));

    struct Scored {
        const nlohmann::json* item;
        int score;
    };
    std::vector<Scored> scored;
    for (const auto& item : items) {
        const std::string status = item.value("status", "");
        if (status == "completed" || status == "cancelled")
            continue;
        const std::string start = item.value("start_time", "");
        if (start.size() < 10)
            continue;
        if (!type_hint.empty() && item.value("type", "event") != type_hint)
            continue;
        if (!source_date.empty() && start.substr(0, 10) != source_date)
            continue;
        const std::string title = lower_of(item.value("title", ""));
        int score = 0;
        for (const auto& w : words)
            if (title.find(w) != std::string::npos)
                score++;
        scored.push_back({&item, score});
    }

    int best = 0;
    for (const auto& s : scored)
        best = std::max(best, s.score);
    std::vector<const nlohmann::json*> picked;
    for (const auto& s : scored) {
        if (best > 0 ? s.score == best
                     // Nothing named: only what the user pinned down by day, or else today's items.
                     : (!source_date.empty() || s.item->value("start_time", "").substr(0, 10) == today))
            picked.push_back(s.item);
    }
    std::sort(picked.begin(), picked.end(), [](const nlohmann::json* a, const nlohmann::json* b) {
        return a->value("start_time", "") < b->value("start_time", "");
    });

    for (const auto* item : picked) {
        if (out.size() == 3)
            break;
        const auto start = parse_stamp(item->value("start_time", ""));
        if (!start)
            continue;
        std::tm moved = *start;
        if (target_day) {
            moved.tm_year = target_day->tm_year;
            moved.tm_mon = target_day->tm_mon;
            moved.tm_mday = target_day->tm_mday;
        }
        if (target_time) {
            moved.tm_hour = std::stoi(target_time->substr(0, 2));
            moved.tm_min = std::stoi(target_time->substr(3, 2));
        }
        moved.tm_isdst = -1;
        std::tm start_copy = *start;
        const std::time_t from = std::mktime(&start_copy);
        const std::time_t to = std::mktime(&moved);

        QuickMoveOption option;
        option.id = item->value("id", "");
        option.title = item->value("title", "");
        option.type = item->value("type", "event");
        option.start_time = item->value("start_time", "");
        option.all_day = item->value("all_day", false);
        option.new_start_time = format_stamp(moved);
        // Keep the item's length: its end moves by as much as its start.
        const std::string end = item->value("end_time", "");
        if (auto end_tm = parse_stamp(end)) {
            std::time_t end_t = std::mktime(&*end_tm) + (to - from);
            std::tm shifted{};
            local_time(&end_t, &shifted);
            option.new_end_time = format_stamp(shifted);
        }
        if (option.new_start_time.substr(0, 16) == option.start_time.substr(0, 16))
            continue; // already there
        out.push_back(option);
    }
    return out;
}

nlohmann::json suggest_quick_action(const std::string& text, std::time_t now,
                                    const std::vector<nlohmann::json>& items) {
    const auto moves = parse_quick_move(text, now, items);
    if (!moves.empty()) {
        nlohmann::json options = nlohmann::json::array();
        for (const auto& m : moves)
            options.push_back(m.to_json());
        return {{"kind", "move"}, {"options", options}};
    }
    if (auto add = parse_quick_add(text, now)) {
        nlohmann::json j = add->to_json();
        j["kind"] = "add";
        return j;
    }
    return nullptr;
}

nlohmann::json QuickAddCandidate::to_json() const {
    return {{"title", title}, {"type", type}, {"start_time", start_time}, {"all_day", all_day}};
}

std::optional<QuickAddCandidate> parse_quick_add(const std::string& text, std::time_t now) {
    if (text.empty() || text.size() > 300)
        return std::nullopt;
    const std::string lower = lower_of(text);

    // Changing an existing item is parse_quick_move's job; offering to add it would duplicate it.
    if (matches(lower, kMoveVerbs))
        return std::nullopt;

    const bool command = matches(lower, "\\b(add|schedule|book|remind me|create|set up|put)\\b");
    // Questions are for the model to answer, unless they are a request to add something.
    if (!command && matches(lower, "^\\s*(what|how|why|who|which|where|when|is|are|was|were|do|does|did|should|could|"
                                   "would|can i|tell me|explain)\\b"))
        return std::nullopt;

    static const std::string kTaskCues = "remind me|need to|have to|must|don'?t forget|todo|to-do|task|submit|due|"
                                         "deadline|pay|buy|finish";
    static const std::string kEventCues =
        "attend(?:ing)?|meeting|meet(?:ing)? with|call|appointment|event|class|lecture|interview|dinner|lunch|"
        "breakfast|party|conference|workshop|session|webinar|seminar|flight|trip|visit|stand-?up|gym|workout|game|"
        "match|concert|wedding|birthday|exam|demo|presentation|dentist|doctor|going to|heading to";
    const bool task_cue = matches(lower, "\\b(" + kTaskCues + ")\\b");
    const bool event_cue = matches(lower, "\\b(" + kEventCues + ")\\b");
    if (!command && !task_cue && !event_cue)
        return std::nullopt;

    const auto day = find_day(lower, now);
    const auto time = find_time(lower);
    if (!day && !time)
        return std::nullopt;

    QuickAddCandidate candidate;
    candidate.type = task_cue && !event_cue ? "task" : "event";
    candidate.title = clean_title(text);
    if (!plausible_title(candidate.title))
        return std::nullopt;

    // A bare time means the next time the clock shows it: today, or tomorrow if it has passed.
    std::tm when = day ? *day : day_offset(now, 0);
    if (!day && time) {
        std::tm current{};
        local_time(&now, &current);
        char hm[6];
        std::snprintf(hm, sizeof(hm), "%02d:%02d", current.tm_hour, current.tm_min);
        if (*time < std::string(hm))
            when = day_offset(now, 1);
    }
    char date[16];
    std::strftime(date, sizeof(date), "%Y-%m-%d", &when);
    candidate.all_day = !time;
    candidate.start_time = std::string(date) + "T" + (time ? *time : std::string("00:00")) + ":00";
    return candidate;
}

} // namespace agent
} // namespace delta
