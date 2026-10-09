## Downloads

Download the installer for your platform from the assets below.

### Which file to download

| File | OS | Notes |
|------|----|-------|
| `.dmg` | macOS | Choose ARM64 for Apple Silicon or x64 for Intel |
| `.deb` | Linux (Ubuntu/Debian) | Install with `sudo dpkg -i` |
| `.rpm` | Linux (Fedora) | Installs desktop dependencies |
| `.AppImage` | Linux | Portable; host desktop libraries required |
| `.msi` or `.exe` | Windows | Standard installer |
| `delta-cli-*.tar.gz` or `.zip` | Command line (CLI) | See **Install the command-line tool (CLI)** below |

### macOS setup (unsigned builds only)

1. Open the `.dmg` and drag **Delta** into **Applications**.
2. If macOS blocks Delta, dismiss the warning.
3. Open **System Settings > Privacy & Security**, scroll down, and click **Open Anyway** for Delta.
4. Confirm by clicking **Open**.

Alternatively, remove the quarantine flag in Terminal:

```sh
sudo xattr -d com.apple.quarantine /Applications/Delta.app
```

### Linux AppImage setup

1. Make the file executable: `chmod +x Delta_*.AppImage`.
2. Prefer the `.rpm` on Fedora or the `.deb` on Ubuntu/Debian so desktop dependencies are installed automatically.
3. For the portable AppImage, install the host WebKitGTK 4.1, GTK3, and EGL/GL runtime libraries.
4. Run it: `./Delta_*.AppImage`.
5. For a driver-specific rendering issue, retry with `WEBKIT_DISABLE_COMPOSITING_MODE=1`.

## Install the command-line tool (CLI)

The CLI gives you the `delta` command in a terminal. With it you can download models, chat, and run
the web UI and an OpenAI-compatible API. You do not need it to use the desktop app.

### 1. Pick your file

| Your computer | Download |
|---|---|
| Mac with an Apple chip (M1, M2, M3, M4) | `delta-cli-macos-arm64.tar.gz` |
| Mac with an Intel processor | `delta-cli-macos-x86_64.tar.gz` |
| Linux, 64-bit (Ubuntu 22.04 or newer, Debian 12, Fedora 38+) | `delta-cli-linux-x86_64.tar.gz` |
| Windows 10 or 11, 64-bit | `delta-cli-windows-x64.zip` |

Not sure which Mac you have? Open the Apple menu > **About This Mac**. "Chip: Apple M…" means Apple chip;
"Processor: Intel…" means Intel.

The download is a folder of programs, not an installer, so double-clicking it does nothing useful.
Follow the steps below for your system. They copy the folder to your home directory and let your
terminal find `delta`. Keep the folder together: `delta` uses the other programs inside it.

### 2a. macOS

Open **Terminal** (press ⌘ Space, type *Terminal*, press Enter) and paste these lines. If you have
an Intel Mac, change `arm64` to `x86_64` in the first line.

```sh
FILE=~/Downloads/delta-cli-macos-arm64.tar.gz
mkdir -p ~/.local/share/delta-cli
tar -xzf "$FILE" -C ~/.local/share/delta-cli --strip-components=1
xattr -dr com.apple.quarantine ~/.local/share/delta-cli    # lets macOS run the unsigned programs
echo 'export PATH="$HOME/.local/share/delta-cli/bin:$PATH"' >> ~/.zshrc
```

**Close Terminal and open it again**, then check that it worked:

```sh
delta --version
```

### 2b. Linux

Open a terminal and paste:

```sh
sudo apt install -y curl        # Ubuntu/Debian: provides libcurl, the one library delta needs
FILE=~/Downloads/delta-cli-linux-x86_64.tar.gz
mkdir -p ~/.local/share/delta-cli
tar -xzf "$FILE" -C ~/.local/share/delta-cli --strip-components=1
echo 'export PATH="$HOME/.local/share/delta-cli/bin:$PATH"' >> ~/.bashrc
```

On Fedora, skip the first line (libcurl is already installed). **Close the terminal and open it
again**, then run `delta --version`.

### 2c. Windows

Open **PowerShell** (Start menu, type *PowerShell*, press Enter) and paste:

```powershell
$zip  = "$HOME\Downloads\delta-cli-windows-x64.zip"
$dest = "$env:LOCALAPPDATA\Programs\delta-cli"
Remove-Item "$env:TEMP\delta-cli" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive $zip -DestinationPath "$env:TEMP\delta-cli" -Force
Remove-Item $dest -Recurse -Force -ErrorAction SilentlyContinue
Move-Item (Get-ChildItem "$env:TEMP\delta-cli\delta-cli-*" | Select-Object -First 1).FullName $dest
Get-ChildItem $dest -Recurse | Unblock-File
$path = [Environment]::GetEnvironmentVariable("Path", "User")
if ($path -notlike "*$dest\bin*") { [Environment]::SetEnvironmentVariable("Path", "$path;$dest\bin", "User") }
```

**Close PowerShell and open it again**, then run `delta --version`.

### 3. First steps

```sh
delta pull qwen2.5:0.5b          # download a small model (about 400 MB); run once
delta                            # open the chat in your browser, with a chat prompt in the terminal too
delta -m qwen2.5:0.5b "Explain photosynthesis in one sentence"   # one answer, right in the terminal
delta --server                   # web UI plus an OpenAI-compatible API on http://127.0.0.1:8081/v1
```

| Command | What it does |
|---|---|
| `delta --list-models` | Models you have downloaded |
| `delta --list-models --available` | Models you can download (`delta pull <name>`) |
| `delta remove <name>` | Delete a downloaded model |
| `delta --update` | Update the CLI in place to the newest release |
| `delta --help` | Every option |

Models are saved in `~/.delta-cli/models` (on Windows, `%USERPROFILE%\.delta-cli\models`) and are
shared with the desktop app.

### If something goes wrong

- **`delta: command not found`** (or *not recognized* on Windows): open a new terminal window. If it
  still fails, run the install lines again; the last one is what makes `delta` findable.
- **macOS says "cannot be opened because the developer cannot be verified"**: run the `xattr` line
  from step 2a again.
- **Linux: `libcurl.so.4: cannot open shared object file`**: run `sudo apt install -y curl`.
- **Windows: "Windows protected your PC"**: run the `Unblock-File` line from step 2c again.
- **Check the download** (optional): compare `shasum -a 256 <file>` (macOS/Linux) or
  `Get-FileHash <file>` (Windows) with the matching line in the `checksums-*.sha256` asset.
- **Uninstall:** delete `~/.local/share/delta-cli` (Windows: `%LOCALAPPDATA%\Programs\delta-cli`)
  and remove its line from `~/.zshrc` or `~/.bashrc`. Your models stay in `~/.delta-cli`.

