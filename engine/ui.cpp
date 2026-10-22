/**
 * UI Module - Retro green terminal styling for Delta CLI
 */

#include "delta_cli.h"
#include <iostream>
#include <string>
#include <cstdio>
#include <locale>
#include <iomanip>
#include <sstream>
#include <ctime>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <cerrno>
#endif
#include <vector>

namespace delta {

void UI::init() {
#ifdef _WIN32
    // Enable ANSI escape codes on Windows
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    // Set UTF-8 code page on Windows for full Unicode support
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Set console font to support Unicode characters
    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof(cfi);
    cfi.nFont = 0;
    cfi.dwFontSize.X = 0;
    cfi.dwFontSize.Y = 16;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
    wcscpy_s(cfi.FaceName, L"Consolas");
    SetCurrentConsoleFontEx(hOut, FALSE, &cfi);
#endif

    // Set locale for internationalization with UTF-8 support
    try {
        // Try to set system locale first
        std::locale::global(std::locale(""));

        // Ensure UTF-8 encoding for output streams
        std::cout.imbue(std::locale(""));
        std::cerr.imbue(std::locale(""));

    } catch (const std::exception&) {
        // Fallback to UTF-8 locale if system locale is not available
        try {
            std::locale::global(std::locale("en_US.UTF-8"));
            std::cout.imbue(std::locale("en_US.UTF-8"));
            std::cerr.imbue(std::locale("en_US.UTF-8"));
        } catch (const std::exception&) {
            // Final fallback to C locale
            std::locale::global(std::locale("C"));
        }
    }
}

void UI::print_banner() {
    // Print static Delta logo ASCII art
    print_delta_logo_ascii();
}
// Δ

void UI::print_prompt() {
#ifdef _WIN32
    std::cout << DELTA_RED << BOLD << "delta> " << RESET << std::flush;
#else
    std::cout << DELTA_RED << BOLD << "δ> " << RESET << std::flush;
#endif
}

void UI::print_response(const std::string& text) {
    std::cout << GREEN << text << RESET;
}

void UI::print_error(const std::string& error) {
#ifdef _WIN32
    std::cout << RED << "Error: " << error << RESET << std::endl;
#else
    std::cout << RED << "✗ Error: " << error << RESET << std::endl;
#endif
}

void UI::print_info(const std::string& info) {
#ifdef _WIN32
    std::cout << YELLOW << ">> " << info << RESET << std::endl;
#else
    std::cout << YELLOW << "ℹ " << info << RESET << std::endl;
#endif
}

void UI::print_warning(const std::string& warning) {
#ifdef _WIN32
    std::cout << YELLOW << "[!] " << warning << RESET << std::endl;
#else
    std::cout << YELLOW << "⚠ " << warning << RESET << std::endl;
#endif
}

void UI::print_success(const std::string& success) {
#ifdef _WIN32
    std::cout << GREEN << "[OK] " << success << RESET << std::endl;
#else
    std::cout << GREEN << "✓ " << success << RESET << std::endl;
#endif
}

void UI::print_border(const std::string& title) {
    int terminal_width = 64;

#ifndef _WIN32
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        terminal_width = w.ws_col;
    }
#else
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        terminal_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
#endif

    std::cout << BRIGHT_GREEN << BOLD;

#ifdef _WIN32
    // ASCII-only on Windows to avoid garbled box-drawing (e.g. â) when console isn't UTF-8
    if (title.empty()) {
        std::cout << "+";
        for (int i = 0; i < terminal_width - 2; i++)
            std::cout << "=";
        std::cout << "+" << std::endl;
    } else {
        int padding = (terminal_width - static_cast<int>(title.length()) - 4) / 2;
        if (padding < 0)
            padding = 0;
        std::cout << "+";
        for (int i = 0; i < padding; i++)
            std::cout << "=";
        std::cout << " " << title << " ";
        int remaining = terminal_width - padding - static_cast<int>(title.length()) - 4;
        if (remaining < 0)
            remaining = 0;
        for (int i = 0; i < remaining; i++)
            std::cout << "=";
        std::cout << "+" << std::endl;
    }
#else
    if (title.empty()) {
        std::cout << "╔";
        for (int i = 0; i < terminal_width - 2; i++)
            std::cout << "═";
        std::cout << "╗" << std::endl;
    } else {
        int padding = (terminal_width - title.length() - 4) / 2;
        std::cout << "╔";
        for (int i = 0; i < padding; i++)
            std::cout << "═";
        std::cout << " " << title << " ";
        int remaining = terminal_width - padding - static_cast<int>(title.length()) - 4;
        for (int i = 0; i < remaining; i++)
            std::cout << "═";
        std::cout << "╗" << std::endl;
    }
#endif

    std::cout << RESET;
}

void UI::clear_line() {
    std::cout << "\r\033[K" << std::flush;
}

#ifndef _WIN32
// Slash commands offered by Tab; keep in step with Commands::show_help().
static const char* const kSlashCommands[] = {"/download", "/remove",       "/delete",   "/list",   "/available",
                                             "/use",      "/clear-screen", "/help",     "/memory", "/forget",
                                             "/plan",     "/tools",        "/policies", "/clear"};

static bool is_continuation(char c) {
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

static size_t columns(const std::string& text, size_t from, size_t to) {
    size_t n = 0;
    for (size_t i = from; i < to; i++)
        n += !is_continuation(text[i]);
    return n;
}

static size_t prev_char(const std::string& text, size_t pos) {
    while (pos > 0 && is_continuation(text[--pos]))
        ;
    return pos;
}

static size_t next_char(const std::string& text, size_t pos) {
    while (pos < text.size() && is_continuation(text[++pos]))
        ;
    return pos;
}

static void redraw(const std::string& line, size_t cursor) {
    std::cout << "\r";
    UI::print_prompt();
    std::cout << line << "\033[K";
    const size_t back = columns(line, cursor, line.size());
    if (back > 0)
        std::cout << "\033[" << back << "D";
    std::cout << std::flush;
}

// Completes a lone /command word: one match fills it in, several fill their shared prefix and are listed.
static void complete_command(std::string& line, size_t& cursor) {
    if (cursor != line.size() || line.empty() || line[0] != '/' || line.find(' ') != std::string::npos)
        return;
    std::vector<std::string> matches;
    for (const char* cmd : kSlashCommands)
        if (std::string(cmd).rfind(line, 0) == 0)
            matches.emplace_back(cmd);
    if (matches.empty())
        return;
    std::string common = matches[0];
    for (const auto& m : matches) {
        size_t i = 0;
        while (i < common.size() && i < m.size() && common[i] == m[i])
            i++;
        common.resize(i);
    }
    if (matches.size() == 1)
        common += " ";
    if (common.size() == line.size() && matches.size() > 1) {
        std::cout << "\n";
        for (const auto& m : matches)
            std::cout << m << "  ";
        std::cout << "\n";
    }
    line = common;
    cursor = line.size();
}

// Lines entered this session, for Up and Down.
static std::vector<std::string> g_history;

// A small line editor, so the prompt behaves like a shell: arrows, history, Home/End, Tab completion.
static bool read_line_raw(std::string& line) {
    termios saved{};
    if (tcgetattr(STDIN_FILENO, &saved) != 0)
        return false;
    termios raw = saved;
    raw.c_lflag &= ~(ICANON | ECHO | IEXTEN); // ISIG stays on so Ctrl-C still interrupts
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    size_t cursor = 0;
    size_t recall = g_history.size();
    std::string draft;
    bool ok = true;
    auto read_byte = [](char& c) { return read(STDIN_FILENO, &c, 1) == 1; };
    auto show = [&](const std::string& text) {
        line = text;
        cursor = line.size();
        redraw(line, cursor);
    };
    while (true) {
        char c = 0;
        const ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n < 0 && errno == EINTR) {
            line.clear();
            break;
        }
        if (n <= 0 || (c == 4 && line.empty())) { // EOF, or Ctrl-D on an empty line
            ok = false;
            break;
        }
        switch (c) {
        case '\r':
        case '\n':
            std::cout << "\n" << std::flush;
            if (!line.empty() && (g_history.empty() || g_history.back() != line))
                g_history.push_back(line);
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
            return true;
        case 127:
        case 8: // Backspace
            if (cursor > 0) {
                const size_t from = prev_char(line, cursor);
                line.erase(from, cursor - from);
                cursor = from;
            }
            break;
        case 4: // Ctrl-D deletes under the cursor
            if (cursor < line.size())
                line.erase(cursor, next_char(line, cursor) - cursor);
            break;
        case '\t':
            complete_command(line, cursor);
            break;
        case 1: // Ctrl-A
            cursor = 0;
            break;
        case 5: // Ctrl-E
            cursor = line.size();
            break;
        case 21: // Ctrl-U
            line.erase(0, cursor);
            cursor = 0;
            break;
        case 11: // Ctrl-K
            line.erase(cursor);
            break;
        case 23: { // Ctrl-W deletes the word before the cursor
            size_t from = cursor;
            while (from > 0 && line[from - 1] == ' ')
                from--;
            while (from > 0 && line[from - 1] != ' ')
                from--;
            line.erase(from, cursor - from);
            cursor = from;
            break;
        }
        case 12: // Ctrl-L
            std::cout << "\033[H\033[2J";
            break;
        case 27: { // escape sequences: arrows, Home, End, Delete
            char a = 0, b = 0;
            if (!read_byte(a) || (a != '[' && a != 'O') || !read_byte(b))
                break;
            if (b >= '0' && b <= '9') {
                char end = 0;
                while (read_byte(end) && !(end >= '@' && end <= '~'))
                    ;
                if (b == '1' || b == '7')
                    cursor = 0;
                else if (b == '4' || b == '8')
                    cursor = line.size();
                else if (b == '3' && cursor < line.size())
                    line.erase(cursor, next_char(line, cursor) - cursor);
            } else if (b == 'D' && cursor > 0) {
                cursor = prev_char(line, cursor);
            } else if (b == 'C' && cursor < line.size()) {
                cursor = next_char(line, cursor);
            } else if (b == 'H') {
                cursor = 0;
            } else if (b == 'F') {
                cursor = line.size();
            } else if (b == 'A' && recall > 0) {
                if (recall == g_history.size())
                    draft = line;
                show(g_history[--recall]);
                continue;
            } else if (b == 'B' && recall < g_history.size()) {
                ++recall;
                show(recall == g_history.size() ? draft : g_history[recall]);
                continue;
            }
            break;
        }
        default:
            if (static_cast<unsigned char>(c) < 32)
                continue;
            line.insert(cursor, 1, c);
            cursor++;
            break;
        }
        redraw(line, cursor);
    }
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
    return ok;
}
#endif

std::string UI::get_input() {
    std::string input;
#ifndef _WIN32
    if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
        if (!read_line_raw(input))
            std::cin.setstate(std::ios::eofbit);
        return input;
    }
#endif
    std::getline(std::cin, input);
    return input;
}

// ============================================================================
// INTERNATIONALIZATION SUPPORT
// ============================================================================

std::string UI::format_size(long long bytes) {
    std::ostringstream oss;
    oss.imbue(std::locale(""));

    if (bytes < 1024) {
        oss << bytes << " B";
    } else if (bytes < 1024 * 1024) {
        oss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        oss << std::fixed << std::setprecision(1) << (bytes / (1024.0 * 1024.0)) << " MB";
    } else {
        oss << std::fixed << std::setprecision(1) << (bytes / (1024.0 * 1024.0 * 1024.0)) << " GB";
    }

    return oss.str();
}

std::string UI::format_number(long long number) {
    std::ostringstream oss;
    oss.imbue(std::locale(""));
    oss << number;
    return oss.str();
}

void UI::print_utf8(const std::string& text) {
    // Ensure UTF-8 output is properly handled
    std::cout << text << std::flush;
}

void UI::print_multilingual_info(const std::string& key, const std::string& value) {
    std::cout << GREEN << "ℹ " << key << ": " << RESET << value << std::endl;
}

void UI::print_multilingual_welcome() {
    // Detect system language and show appropriate welcome message
    std::string lang = get_system_language();

    if (lang == "zh" || lang == "zh-CN" || lang == "zh-TW") {
        std::cout << GREEN << "欢迎使用 Delta CLI！" << RESET << std::endl;
        std::cout << YELLOW << "支持中文对话，请开始输入..." << RESET << std::endl;
    } else if (lang == "ja" || lang == "ja-JP") {
        std::cout << GREEN << "Delta CLI へようこそ！" << RESET << std::endl;
        std::cout << YELLOW << "日本語での会話をサポートしています..." << RESET << std::endl;
    } else if (lang == "ko" || lang == "ko-KR") {
        std::cout << GREEN << "Delta CLI에 오신 것을 환영합니다!" << RESET << std::endl;
        std::cout << YELLOW << "한국어 대화를 지원합니다..." << RESET << std::endl;
    } else if (lang == "es" || lang == "es-ES" || lang == "es-MX") {
        std::cout << GREEN << "¡Bienvenido a Delta CLI!" << RESET << std::endl;
        std::cout << YELLOW << "Soporta conversación en español..." << RESET << std::endl;
    } else if (lang == "fr" || lang == "fr-FR" || lang == "fr-CA") {
        std::cout << GREEN << "Bienvenue dans Delta CLI !" << RESET << std::endl;
        std::cout << YELLOW << "Supporte la conversation en français..." << RESET << std::endl;
    } else if (lang == "de" || lang == "de-DE") {
        std::cout << GREEN << "Willkommen bei Delta CLI!" << RESET << std::endl;
        std::cout << YELLOW << "Unterstützt deutsche Gespräche..." << RESET << std::endl;
    } else {
        std::cout << GREEN << "Welcome to Delta CLI!" << RESET << std::endl;
        std::cout << YELLOW << "Supports multilingual conversations..." << RESET << std::endl;
    }
}

std::string UI::get_system_language() {
    // Get system language from environment variables
    const char* lang = std::getenv("LANG");
    if (lang) {
        std::string lang_str(lang);
        // Extract language code (e.g., "en_US.UTF-8" -> "en")
        size_t pos = lang_str.find('_');
        if (pos != std::string::npos) {
            return lang_str.substr(0, pos);
        }
        pos = lang_str.find('.');
        if (pos != std::string::npos) {
            return lang_str.substr(0, pos);
        }
        return lang_str;
    }

    // Fallback to LC_ALL
    lang = std::getenv("LC_ALL");
    if (lang) {
        std::string lang_str(lang);
        size_t pos = lang_str.find('_');
        if (pos != std::string::npos) {
            return lang_str.substr(0, pos);
        }
        return lang_str;
    }

    return "en"; // Default to English
}

void UI::clear_screen() {
    // Use ANSI escape codes to clear screen without spawning a shell
    std::cout << "\033[2J\033[H" << std::flush;
}

void UI::print_history_entry(const std::string& timestamp, const std::string& user_msg, const std::string& ai_resp,
                             const std::string& model) {
    // Clean, professional history display
    std::cout << YELLOW << "[" << timestamp << "] " << RESET;
    std::cout << BRIGHT_GREEN << "User: " << RESET << user_msg << std::endl;

    // Format AI response with proper indentation and line wrapping
    std::cout << "        " << GREEN << "Delta (" << model << "): " << RESET;

    // Simple word wrapping for better readability
    std::string wrapped_response = ai_resp;
    size_t max_line_length = 70;
    size_t current_pos = 0;

    while (current_pos < wrapped_response.length()) {
        size_t line_end = current_pos + max_line_length;
        if (line_end >= wrapped_response.length()) {
            line_end = wrapped_response.length();
        } else {
            // Find last space before line_end
            size_t last_space = wrapped_response.rfind(' ', line_end);
            if (last_space != std::string::npos && last_space > current_pos) {
                line_end = last_space;
            }
        }

        std::cout << wrapped_response.substr(current_pos, line_end - current_pos);
        if (line_end < wrapped_response.length()) {
            std::cout << std::endl << "        ";
        }
        current_pos = line_end;
        if (current_pos < wrapped_response.length() && wrapped_response[current_pos] == ' ') {
            current_pos++; // Skip the space
        }
    }

    std::cout << std::endl << std::endl;
}

void UI::print_session_info(const std::string& name, const std::string& created_at, const std::string& last_accessed,
                            int entry_count) {
    std::cout << BRIGHT_GREEN << "• " << name << RESET << std::endl;
    std::cout << "  Created: " << created_at << std::endl;
    std::cout << "  Last accessed: " << last_accessed << std::endl;
    std::cout << "  Entries: " << entry_count << std::endl;
    std::cout << std::endl;
}

// Enhanced logo and banner functionality
int UI::get_terminal_width() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80; // Default fallback
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80; // Default fallback
#endif
}

bool UI::has_color_support() {
    // Check if terminal supports colors
    const char* term = std::getenv("TERM");
    if (!term)
        return false;

    // Check for color support in TERM variable
    std::string term_str(term);
    if (term_str.find("color") != std::string::npos || term_str.find("256") != std::string::npos ||
        term_str.find("xterm") != std::string::npos || term_str.find("screen") != std::string::npos) {
        return true;
    }

    // Check for NO_COLOR environment variable
    if (std::getenv("NO_COLOR"))
        return false;

    // Check for COLORTERM
    if (std::getenv("COLORTERM"))
        return true;

    return false;
}

void UI::print_delta_logo_ascii() {
    bool color_support = has_color_support();

    if (color_support) {
        std::cout << DELTA_RED << BOLD << R"(
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║ ██████╗ ███████╗██╗  ████████╗ █████╗      ██████╗██╗     ██╗ ║
║ ██╔══██╗██╔════╝██║  ╚══██╔══╝██╔══██╗    ██╔════╝██║     ██║ ║
║ ██║  ██║█████╗  ██║     ██║   ███████║    ██║     ██║     ██║ ║
║ ██║  ██║██╔══╝  ██║     ██║   ██╔══██║    ██║     ██║     ██║ ║
║ ██████╔╝███████╗███████╗██║   ██║  ██║    ╚██████╗███████╗██║ ║
║ ╚═════╝ ╚══════╝╚══════╝╚═╝   ╚═╝  ╚═╝     ╚═════╝╚══════╝╚═╝ ║
║                                                               ║
║                Offline AI Assistant — Delta CLI               ║
║                         Version 1.0.0                         ║
╚═══════════════════════════════════════════════════════════════╝
)" << RESET << std::endl;
    } else {
        // Monochrome fallback
        std::cout << BOLD << R"(
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║ ██████╗ ███████╗██╗  ████████╗ █████╗      ██████╗██╗     ██╗ ║
║ ██╔══██╗██╔════╝██║  ╚══██╔══╝██╔══██╗    ██╔════╝██║     ██║ ║
║ ██║  ██║█████╗  ██║     ██║   ███████║    ██║     ██║     ██║ ║
║ ██║  ██║██╔══╝  ██║     ██║   ██╔══██║    ██║     ██║     ██║ ║
║ ██████╔╝███████╗███████╗██║   ██║  ██║    ╚██████╗███████╗██║ ║
║ ╚═════╝ ╚══════╝╚══════╝╚═╝   ╚═╝  ╚═╝     ╚═════╝╚══════╝╚═╝ ║
║                                                               ║
║                Offline AI Assistant — Delta CLI               ║
║                         Version 1.0.0                         ║
╚═══════════════════════════════════════════════════════════════╝
)" << RESET << std::endl;
    }
}

} // namespace delta
