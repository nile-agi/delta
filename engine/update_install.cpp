#include "update_install.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace delta {
namespace update {

namespace {

// --- SHA-256 (FIPS 180-4) ---------------------------------------------------------------------

class Sha256 {
  public:
    Sha256() { reset(); }

    void update(const unsigned char* data, size_t len) {
        for (size_t i = 0; i < len; i++) {
            block_[block_len_++] = data[i];
            if (block_len_ == 64) {
                transform();
                bit_len_ += 512;
                block_len_ = 0;
            }
        }
    }

    std::string hex() {
        uint64_t total_bits = bit_len_ + block_len_ * 8ull;
        block_[block_len_++] = 0x80;
        if (block_len_ > 56) {
            while (block_len_ < 64)
                block_[block_len_++] = 0;
            transform();
            block_len_ = 0;
        }
        while (block_len_ < 56)
            block_[block_len_++] = 0;
        for (int i = 7; i >= 0; i--)
            block_[block_len_++] = static_cast<unsigned char>(total_bits >> (i * 8));
        transform();

        static const char* digits = "0123456789abcdef";
        std::string out;
        for (uint32_t word : state_) {
            for (int shift = 28; shift >= 0; shift -= 4)
                out += digits[(word >> shift) & 0xf];
        }
        reset();
        return out;
    }

  private:
    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void reset() {
        state_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        block_len_ = 0;
        bit_len_ = 0;
    }

    void transform() {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = (uint32_t(block_[i * 4]) << 24) | (uint32_t(block_[i * 4 + 1]) << 16) |
                   (uint32_t(block_[i * 4 + 2]) << 8) | uint32_t(block_[i * 4 + 3]);
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<uint32_t, 8> state_{};
    unsigned char block_[64]{};
    size_t block_len_ = 0;
    uint64_t bit_len_ = 0;
};

std::string lower(std::string s) {
    for (auto& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string exe_name(const std::string& base) {
#ifdef _WIN32
    return base + ".exe";
#else
    return base;
#endif
}

// Quotes a path for the shell. Paths here are built by the updater itself (a temp folder and a
// fixed archive name), but quoting keeps folders with spaces working.
std::string quoted(const std::string& path) {
#ifdef _WIN32
    return "\"" + path + "\"";
#else
    std::string out = "'";
    for (char c : path)
        out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return out + "'";
#endif
}

bool unpack(const std::string& archive, const std::string& into) {
    // bsdtar ships with macOS and Windows 10+ and reads both .tar.gz and .zip; GNU tar on Linux
    // reads the .tar.gz archives built for Linux.
    const bool zip = archive.size() > 4 && lower(archive.substr(archive.size() - 4)) == ".zip";
    std::string cmd = "tar -x" + std::string(zip ? "" : "z") + "f " + quoted(archive) + " -C " + quoted(into);
#ifndef _WIN32
    cmd += " 2>/dev/null";
#endif
    return std::system(cmd.c_str()) == 0;
}

// One path to replace: `source` is copied to `staged`, which then takes `target`'s place while
// the old one waits at `backup`.
struct Swap {
    fs::path source, staged, target, backup;
    bool had_old = false;
    bool done = false;
};

void undo(std::vector<Swap>& swaps) {
    std::error_code ec;
    for (auto it = swaps.rbegin(); it != swaps.rend(); ++it) {
        if (!it->done)
            continue;
        fs::remove_all(it->target, ec);
        if (it->had_old)
            fs::rename(it->backup, it->target, ec);
    }
    for (auto& s : swaps)
        fs::remove_all(s.staged, ec);
}

} // namespace

std::string sha256_hex(const std::string& data) {
    Sha256 h;
    h.update(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    return h.hex();
}

std::string sha256_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return "";
    Sha256 h;
    std::array<char, 1 << 16> buf{};
    while (in) {
        in.read(buf.data(), buf.size());
        if (in.gcount() > 0)
            h.update(reinterpret_cast<const unsigned char*>(buf.data()), static_cast<size_t>(in.gcount()));
    }
    return in.bad() ? "" : h.hex();
}

std::string checksum_for(const std::string& checksums, const std::string& name) {
    std::istringstream lines(checksums);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream parts(line);
        std::string hash, file;
        parts >> hash >> file;
        if (!file.empty() && file[0] == '*') // binary-mode marker
            file.erase(0, 1);
        if (file == name && hash.size() == 64 &&
            std::all_of(hash.begin(), hash.end(), [](unsigned char c) { return std::isxdigit(c); }))
            return lower(hash);
    }
    return "";
}

std::string package_manager_for(const std::string& executable_path) {
    std::string p = executable_path;
    std::replace(p.begin(), p.end(), '\\', '/');
    const std::string l = lower(p);
    if (l.find("/cellar/") != std::string::npos || l.find("/homebrew/") != std::string::npos ||
        l.find("/linuxbrew/") != std::string::npos)
        return "Homebrew";
    if (l.rfind("/nix/store/", 0) == 0)
        return "Nix";
    if (l.rfind("/opt/local/", 0) == 0)
        return "MacPorts";
    if (l.find("/microsoft/winget/") != std::string::npos)
        return "winget";
    return "";
}

std::string upgrade_command(const std::string& manager) {
    if (manager == "Homebrew")
        return "brew upgrade delta-cli";
    if (manager == "MacPorts")
        return "sudo port selfupdate && sudo port upgrade delta-cli";
    if (manager == "Nix")
        return "update the delta-cli package in your Nix configuration";
    if (manager == "winget")
        return "winget upgrade DeltaCLI.DeltaCLI";
    return "";
}

std::string webui_dir_for(const std::string& bin_dir) {
    for (const char* rel : {"../webui", "../share/delta-cli/webui"}) {
        std::error_code ec;
        fs::path p = fs::weakly_canonical(fs::path(bin_dir) / rel, ec);
        if (!ec && fs::is_directory(p, ec))
            return p.string();
    }
    return "";
}

InstallResult install_archive(const std::string& archive, const std::string& work_dir, const std::string& bin_dir,
                              const std::string& webui_dir) {
    InstallResult result;
    std::error_code ec;

    const fs::path unpacked = fs::path(work_dir) / "unpacked";
    fs::remove_all(unpacked, ec);
    fs::create_directories(unpacked, ec);
    if (!unpack(archive, unpacked.string())) {
        result.error = "Could not unpack the downloaded archive.";
        return result;
    }

    // delta-cli-<version>/ holds bin/ and webui/.
    fs::path root;
    for (const auto& entry : fs::directory_iterator(unpacked, ec)) {
        if (entry.is_directory() && fs::exists(entry.path() / "bin" / exe_name("delta")))
            root = entry.path();
    }
    if (root.empty()) {
        result.error = "The archive does not have the expected bin/ folder.";
        return result;
    }

    std::vector<Swap> swaps;
    for (const char* base : {"delta", "delta-server", "llama-server"}) {
        const std::string name = exe_name(base);
        if (!fs::exists(root / "bin" / name)) {
            result.error = "The archive is missing " + name + ".";
            return result;
        }
        const fs::path target = fs::path(bin_dir) / name;
        swaps.push_back({root / "bin" / name, target.string() + ".new", target, target.string() + ".old"});
    }
    if (!webui_dir.empty() && fs::is_directory(root / "webui"))
        swaps.push_back({root / "webui", webui_dir + ".new", webui_dir, webui_dir + ".old"});

    // Stage beside each target, so the final step is a rename on the same filesystem.
    for (auto& s : swaps) {
        fs::remove_all(s.staged, ec);
        fs::remove_all(s.backup, ec); // left over from an earlier update
        fs::copy(s.source, s.staged, fs::copy_options::recursive, ec);
        if (ec) {
            result.error = "Could not write " + s.staged.string() + ": " + ec.message();
            undo(swaps);
            return result;
        }
#ifndef _WIN32
        if (fs::is_regular_file(s.staged))
            fs::permissions(s.staged, fs::perms(0755), ec);
#endif
    }

    // Swap. A running executable can be renamed (Windows included) though not deleted, so the old
    // one moves aside rather than being overwritten.
    for (auto& s : swaps) {
        s.had_old = fs::exists(s.target);
        if (s.had_old)
            fs::rename(s.target, s.backup, ec);
        if (!ec)
            fs::rename(s.staged, s.target, ec);
        if (ec) {
            if (s.had_old && !fs::exists(s.target)) {
                std::error_code restore;
                fs::rename(s.backup, s.target, restore);
            }
            result.error = "Could not replace " + s.target.string() + ": " + ec.message();
            undo(swaps);
            return result;
        }
        s.done = true;
    }

    for (auto& s : swaps) {
        // Fails harmlessly for the running delta.exe on Windows; the next update clears it.
        fs::remove_all(s.backup, ec);
        result.replaced.push_back(s.target.string());
    }
    fs::remove_all(unpacked, ec);
    result.ok = true;
    return result;
}

} // namespace update
} // namespace delta
