#include "quick_add.h"
#include "time_compat.h"
#include "tool_calendar.h"
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

} // namespace

nlohmann::json QuickAddCandidate::to_json() const {
    return {{"title", title}, {"type", type}, {"start_time", start_time}, {"all_day", all_day}};
}

std::optional<QuickAddCandidate> parse_quick_add(const std::string& text, std::time_t now) {
    if (text.empty() || text.size() > 300)
        return std::nullopt;
    const std::string lower = lower_of(text);

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
    if (candidate.title.size() < 2)
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
