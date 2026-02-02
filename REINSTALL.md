# Reinstalling Delta So Your Web UI Changes Are Visible

If you changed the web UI (e.g. Model Management, context length selection) and **reinstalled Delta**, but the old UI still appears, Delta is not using the web UI built from this repo.

## Why this happens

- **`brew reinstall delta-cli`** or **`brew install delta-cli`** uses the formula’s source or a bottle from the tap. That build does **not** include your local changes in `assets/`.
- **Manual install** without building the web UI first installs only the C++ binaries; the web UI under `public/` is never built or copied into the install prefix.

So after “reinstall,” the app may be serving an old or default web UI instead of the one you built.

## Fix: Install from this repo and use the built web UI

From the **root of this repo** (where `assets/` and `CMakeLists.txt` are), run:

```bash
./scripts/build-webui-and-install.sh
```

This script will:

1. Build the web UI from **your** `assets/` into `public/`
2. Build the Delta C++ app in `build/`
3. Install **both** binaries and the built `public/` to `/usr/local` (or the prefix you pass)

After that, when you run `delta` or `delta-server`, they will serve the web UI from your build (including context length selection in Model Management).

### Using a different install prefix

- **Homebrew prefix (Apple Silicon):**
  ```bash
  ./scripts/build-webui-and-install.sh /opt/homebrew
  ```
- **Homebrew prefix (Intel):**
  ```bash
  ./scripts/build-webui-and-install.sh /usr/local
  ```

### Requirements

- **Node.js and npm** (to build the web UI). If missing: `brew install node`
- **CMake, compiler** (to build the C++ app). On macOS: Xcode Command Line Tools (`xcode-select --install`)

## Verifying

1. Run the script from the repo root as above.
2. Open Delta → **Settings** → **Model Management** → **Installed**.
3. You should see:
   - The first installed model **expanded by default** with a **“Context length”** section and radio options (e.g. “4k ctx on 0.3 GB mem”, “32k ctx on 0.4 GB mem”).
   - Same dark theme and layout as the rest of Model Management.

If you still see the old UI (no context length section, different layout), the running Delta binary is not using the web UI from this install. Ensure you’re running the `delta` / `delta-server` that was just installed (e.g. `/usr/local/bin/delta` or `/opt/homebrew/bin/delta`).

## More detail

- Full local install steps: [INSTALL_LOCAL.md](INSTALL_LOCAL.md)
- “Keeping your web UI changes after reinstall”: [INSTALL_LOCAL.md#keeping-your-web-ui-changes-after-reinstall](INSTALL_LOCAL.md#keeping-your-web-ui-changes-after-reinstall)
