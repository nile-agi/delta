#ifndef DELTA_UPDATE_INSTALL_H
#define DELTA_UPDATE_INSTALL_H

#include <string>
#include <vector>

// The offline half of `delta --update`: checking a downloaded archive and swapping it into place.
// Kept free of networking and terminal output so it can be tested on its own.
namespace delta {
namespace update {

// SHA-256 of a byte string or a file, as lowercase hex. "" when the file cannot be read.
std::string sha256_hex(const std::string& data);
std::string sha256_file(const std::string& path);

// The hash listed for `name` in a checksums file ("<hash>  <name>" or "<hash> *<name>" lines,
// '#' comments). "" when it is not listed.
std::string checksum_for(const std::string& checksums, const std::string& name);

// The package manager that owns an executable at this resolved path ("Homebrew", "MacPorts", "Nix",
// "winget"), or "" for a manual install. Those installs must be upgraded with their own tool, or
// it would no longer know what it installed.
std::string package_manager_for(const std::string& executable_path);
std::string upgrade_command(const std::string& manager);

// Where a manual install keeps its web UI, beside its bin/ folder. "" when there is none.
std::string webui_dir_for(const std::string& bin_dir);

struct InstallResult {
    bool ok = false;
    std::string error;
    std::vector<std::string> replaced; // paths now holding the new version
};

// Unpacks a release archive (`delta-cli-<version>/bin/...`, `.../webui/`) into `work_dir` and
// replaces the delta, delta-server and llama-server executables in `bin_dir`, plus the web UI at
// `webui_dir` when that is not empty. Everything is staged first and then swapped; if any swap
// fails, every path already swapped is restored, so the install is never left half old, half new.
InstallResult install_archive(const std::string& archive, const std::string& work_dir, const std::string& bin_dir,
                              const std::string& webui_dir);

} // namespace update
} // namespace delta

#endif // DELTA_UPDATE_INSTALL_H
