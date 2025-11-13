# Uninstall Delta CLI

Instructions for removing Delta CLI from your system.

## 🍎 macOS (Homebrew)

### If installed via Homebrew tap:
```bash
brew uninstall delta
brew untap nile-agi/delta
```

### If installed manually:
```bash
# Remove binaries
rm -f /opt/homebrew/bin/delta /opt/homebrew/bin/delta-server
rm -f /usr/local/bin/delta /usr/local/bin/delta-server

# Remove web UI
rm -rf /opt/homebrew/share/delta-cli
rm -rf /usr/local/share/delta-cli

# Remove PATH configuration from ~/.zshrc or ~/.bash_profile
# Edit the file and remove lines containing "Delta CLI"
nano ~/.zshrc  # or ~/.bash_profile
# Remove lines with "Delta CLI PATH fix" and "alias delta="
```

## 🐧 Linux

### Debian/Ubuntu (if installed via script):
```bash
# Remove binaries
sudo rm -f /usr/local/bin/delta /usr/local/bin/delta-server

# Remove web UI
sudo rm -rf /usr/local/share/delta-cli

# Remove PATH configuration
sudo rm -f /etc/profile.d/delta-cli.sh

# Update environment
sudo sed -i '/\/usr\/local\/bin/d' /etc/environment
```

### RHEL/CentOS/Fedora (if installed via script):
```bash
# Remove binaries
sudo rm -f /usr/local/bin/delta /usr/local/bin/delta-server

# Remove web UI
sudo rm -rf /usr/local/share/delta-cli

# Remove PATH configuration
sudo rm -f /etc/profile.d/delta-cli.sh
```

### Manual cleanup:
```bash
# Check where delta is installed
which delta

# Remove the binary
sudo rm -f $(which delta)
sudo rm -f $(which delta-server)

# Remove any configuration files
sudo rm -rf /usr/local/share/delta-cli
sudo rm -f /etc/profile.d/delta-cli.sh
```

## 🪟 Windows

### If installed via PowerShell script:
```powershell
# Remove installation directory
Remove-Item -Recurse -Force "C:\Program Files\Delta CLI"

# Remove from PATH (requires Administrator)
$currentPath = [Environment]::GetEnvironmentVariable("Path", "Machine")
$newPath = $currentPath -replace ";C:\\Program Files\\Delta CLI", ""
[Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")

# Remove desktop shortcut
Remove-Item "$env:USERPROFILE\Desktop\Delta CLI.lnk" -ErrorAction SilentlyContinue
```

### Manual cleanup:
1. Delete installation folder: `C:\Program Files\Delta CLI`
2. Remove from PATH:
   - Open "Environment Variables" from System Properties
   - Edit "Path" variable
   - Remove `C:\Program Files\Delta CLI`
3. Delete desktop shortcut if it exists

## 🧹 Complete Cleanup

### Remove all traces:

**macOS:**
```bash
# Remove binaries
rm -f /opt/homebrew/bin/delta /opt/homebrew/bin/delta-server
rm -f /usr/local/bin/delta /usr/local/bin/delta-server

# Remove web UI
rm -rf /opt/homebrew/share/delta-cli
rm -rf /usr/local/share/delta-cli

# Remove configuration
rm -rf ~/.delta
rm -rf ~/.config/delta-cli

# Clean shell config (edit manually)
nano ~/.zshrc  # Remove Delta CLI related lines
```

**Linux:**
```bash
# Remove binaries
sudo rm -f /usr/local/bin/delta /usr/local/bin/delta-server

# Remove web UI
sudo rm -rf /usr/local/share/delta-cli

# Remove configuration
rm -rf ~/.delta
rm -rf ~/.config/delta-cli

# Remove PATH config
sudo rm -f /etc/profile.d/delta-cli.sh
```

**Windows:**
```powershell
# Remove installation
Remove-Item -Recurse -Force "C:\Program Files\Delta CLI"

# Remove user data
Remove-Item -Recurse -Force "$env:USERPROFILE\.delta" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "$env:APPDATA\delta-cli" -ErrorAction SilentlyContinue

# Remove from PATH (requires Administrator)
# Use System Properties > Environment Variables
```

## ✅ Verify Uninstallation

After uninstalling, verify:

```bash
# Should show "command not found"
delta --version

# Check if binaries still exist
which delta  # Should return nothing
```

## 🔄 Reinstall

If you want to reinstall later:

**macOS:**
```bash
brew tap nile-agi/delta && brew install delta
```

**Linux:**
```bash
curl -fsSL https://raw.githubusercontent.com/nile-agi/delta/main/packaging/linux/install-deb.sh | sudo bash
```

**Windows:**
```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force; Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nile-agi/delta/main/packaging/windows/install.ps1" -OutFile install.ps1; .\install.ps1
```

