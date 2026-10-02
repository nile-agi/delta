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
| `delta-cli-*.tar.gz` or `.zip` | CLI | Choose your OS and processor architecture; verify the SHA-256 checksum |

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
