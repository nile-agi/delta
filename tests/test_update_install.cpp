/**
 * Tests for the install half of `delta --update`: SHA-256, checksum files, package-manager
 * detection, and swapping a real archive into a scratch install.
 *
 *   cmake --build build --target delta-update-tests && ./build/delta-update-tests
 */

#include "update_install.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace delta::update;

static int g_failures = 0;
static int g_checks = 0;

static void check(bool condition, const std::string& what) {
    g_checks++;
    std::cout << (condition ? "    ok   " : "  FAIL   ") << what << "\n";
    if (!condition)
        g_failures++;
}

static void section(const std::string& name) {
    std::cout << "\n" << name << "\n";
}

static void write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

static std::string read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

#ifdef _WIN32
static const char* EXE = ".exe";
#else
static const char* EXE = "";
#endif

// A release archive with the real layout: delta-cli-<version>/bin/* and webui/.
static fs::path make_archive(const fs::path& dir, const std::string& version, bool with_llama = true) {
    const fs::path root = dir / "src" / ("delta-cli-" + version);
    write(root / "bin" / (std::string("delta") + EXE), "delta " + version);
    write(root / "bin" / (std::string("delta-server") + EXE), "delta-server " + version);
    if (with_llama)
        write(root / "bin" / (std::string("llama-server") + EXE), "llama-server " + version);
    write(root / "webui" / "index.html", "<html>" + version + "</html>");
    const fs::path archive = dir / "delta-cli-test.tar.gz";
    const std::string cmd =
        "tar -czf '" + archive.string() + "' -C '" + (dir / "src").string() + "' delta-cli-" + version;
    check(std::system(cmd.c_str()) == 0, "built a test archive");
    return archive;
}

static fs::path make_install(const fs::path& dir) {
    const fs::path bin = dir / "install" / "bin";
    for (const char* name : {"delta", "delta-server", "llama-server"})
        write(bin / (std::string(name) + EXE), std::string(name) + " old");
    write(dir / "install" / "webui" / "index.html", "<html>old</html>");
    return bin;
}

static bool no_leftovers(const fs::path& dir) {
    for (const auto& e : fs::recursive_directory_iterator(dir)) {
        const std::string n = e.path().filename().string();
        if (n.size() > 4 && (n.substr(n.size() - 4) == ".new" || n.substr(n.size() - 4) == ".old"))
            return false;
    }
    return true;
}

int main() {
    section("SHA-256");
    check(sha256_hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "empty input");
    check(sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "\"abc\"");
    check(sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
          "two-block message");
    check(sha256_hex(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
          "a million 'a's");

    const fs::path tmp = fs::temp_directory_path() / "delta-update-tests";
    fs::remove_all(tmp);
    write(tmp / "file.bin", std::string(200000, 'x') + "tail");
    check(sha256_file((tmp / "file.bin").string()) == sha256_hex(std::string(200000, 'x') + "tail"),
          "a file hashes like its bytes");
    check(sha256_file((tmp / "missing").string()).empty(), "a missing file gives no hash");

    section("checksum files");
    const std::string sums =
        "# Delta CLI v1.0.13\n#\n"
        "8185d2b0af3bc1cccd569656d82f6653ebed08931937ee2be10fbcc9d78a3da2  delta-cli-linux-x86_64.tar.gz\r\n"
        "46D018C88A43AD8D2646C544245AA9268EF03F0902038751FCCBFF397E965153 *delta-cli-windows-x64.zip\n";
    check(checksum_for(sums, "delta-cli-linux-x86_64.tar.gz") ==
              "8185d2b0af3bc1cccd569656d82f6653ebed08931937ee2be10fbcc9d78a3da2",
          "a text-mode line, with CRLF");
    check(checksum_for(sums, "delta-cli-windows-x64.zip") ==
              "46d018c88a43ad8d2646c544245aa9268ef03f0902038751fccbff397e965153",
          "a binary-mode (*) line, lowercased");
    check(checksum_for(sums, "delta-cli-macos-arm64.tar.gz").empty(), "an unlisted file has no checksum");

    section("package-managed installs");
    check(package_manager_for("/opt/homebrew/Cellar/delta-cli/1.0.13/bin/delta") == "Homebrew", "Homebrew (arm)");
    check(package_manager_for("/usr/local/Cellar/delta-cli/1.0.13/bin/delta") == "Homebrew", "Homebrew (intel)");
    check(package_manager_for("/home/linuxbrew/.linuxbrew/Cellar/delta-cli/1.0.13/bin/delta") == "Homebrew",
          "Homebrew on Linux");
    check(package_manager_for("/nix/store/abc-delta-cli-1.0.13/bin/delta") == "Nix", "Nix");
    check(package_manager_for("/opt/local/bin/delta") == "MacPorts", "MacPorts");
    check(package_manager_for("C:\\Users\\a\\AppData\\Local\\Microsoft\\WinGet\\Packages\\Delta\\delta.exe") ==
              "winget",
          "winget");
    check(package_manager_for("/usr/local/bin/delta").empty(), "a manual install");
    check(!upgrade_command("Homebrew").empty() && upgrade_command("").empty(), "upgrade commands");

    section("installing an archive");
    {
        const fs::path dir = tmp / "ok";
        const fs::path bin = make_install(dir);
        const fs::path archive = make_archive(dir, "9.9.9");
        const std::string webui = webui_dir_for(bin.string());
        check(!webui.empty() && fs::equivalent(webui, dir / "install" / "webui"), "the install's web UI is found");
        auto r = install_archive(archive.string(), (dir / "work").string(), bin.string(), webui);
        check(r.ok, "install succeeds" + (r.error.empty() ? "" : ": " + r.error));
        check(read(bin / (std::string("delta") + EXE)) == "delta 9.9.9", "delta replaced");
        check(read(bin / (std::string("llama-server") + EXE)) == "llama-server 9.9.9", "llama-server replaced");
        check(read(dir / "install" / "webui" / "index.html") == "<html>9.9.9</html>", "web UI replaced");
        check(r.replaced.size() == 4, "four paths reported replaced");
        check(no_leftovers(dir / "install"), "no .new or .old files left behind");
#ifndef _WIN32
        check((fs::status(bin / "delta").permissions() & fs::perms::owner_exec) != fs::perms::none,
              "the new delta is executable");
#endif
    }
    {
        const fs::path dir = tmp / "missing";
        const fs::path bin = make_install(dir);
        const fs::path archive = make_archive(dir, "9.9.9", false);
        auto r = install_archive(archive.string(), (dir / "work").string(), bin.string(), "");
        check(!r.ok && r.error.find("llama-server") != std::string::npos, "an incomplete archive is refused");
        check(read(bin / (std::string("delta") + EXE)) == "delta old", "and the install is untouched");
    }
    {
        const fs::path dir = tmp / "garbage";
        const fs::path bin = make_install(dir);
        write(dir / "bad.tar.gz", "not an archive");
        auto r = install_archive((dir / "bad.tar.gz").string(), (dir / "work").string(), bin.string(), "");
        check(!r.ok, "a damaged archive is refused");
        check(read(bin / (std::string("delta") + EXE)) == "delta old", "and the install is untouched");
    }
#ifndef _WIN32
    if (geteuid() != 0) {
        // The web UI's folder cannot be written: staging fails before anything is swapped.
        const fs::path dir = tmp / "readonly";
        const fs::path bin = make_install(dir);
        const fs::path archive = make_archive(dir, "9.9.9");
        const fs::path ui_parent = dir / "locked";
        write(ui_parent / "webui" / "index.html", "<html>old</html>");
        fs::permissions(ui_parent, fs::perms(0555));
        auto r =
            install_archive(archive.string(), (dir / "work").string(), bin.string(), (ui_parent / "webui").string());
        fs::permissions(ui_parent, fs::perms(0755));
        check(!r.ok, "an unwritable web UI folder fails the update");
        check(read(bin / "delta") == "delta old" && read(bin / "llama-server") == "llama-server old",
              "and every binary is still the old one");
        check(no_leftovers(dir / "install"), "with nothing left behind");
    }
#endif

    // The real v1.0.13 archive, when one is on hand (scripts drop it in /tmp/deltapkg).
    const fs::path real = "/tmp/deltapkg/delta-cli-macos-arm64.tar.gz";
    if (fs::exists(real)) {
        section("a real release archive");
        check(sha256_file(real.string()) == "b70d5521c3a4da44badcd4e1cc50298d39553795887e10a17a93db472603616e",
              "matches the published v1.0.13 checksum");
    }

    fs::remove_all(tmp);
    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
