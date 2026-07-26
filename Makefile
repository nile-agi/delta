.PHONY: all engine sidecars web preview dev run clean help submodules ensure-submodules

help:
	@echo "make sidecars - Build C++ engine + copy binaries into src-tauri/binaries/ (needed by the app)"
	@echo "make engine   - Build C++ engine only (into build/, not used directly by the app)"
	@echo "make web      - Build SvelteKit web UI"
	@echo "make preview  - Build web + serve it WITH the engine at http://localhost:8080 (browser)"
	@echo "make dev      - Run Tauri desktop app (auto-builds sidecars if missing)"
	@echo "make run      - Full rebuild then run"
	@echo "make clean    - Remove build artifacts"

# `engine` is omitted on purpose: `sidecars` already builds everything the app
# ships, so including it would just do a second redundant full build.
all: sidecars web

submodules:
	git submodule update --init --recursive

# Init submodules only if the llama.cpp checkout is empty (fresh non-recursive clone).
ensure-submodules:
	@if [ ! -f engine/vendor/llama.cpp/CMakeLists.txt ]; then \
		echo "=== Initializing git submodules (llama.cpp) ==="; \
		git submodule update --init --recursive; \
	fi

engine: ensure-submodules
	@echo "=== Building engine ==="
	@bash scripts/check-toolchain.sh
	mkdir -p build
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=ON
	cmake --build build -j$$(sysctl -n hw.ncpu) --target delta-server --target llama-server

# Builds the sidecars and copies them into src-tauri/binaries/ with the
# target-triple suffix Tauri requires. `make engine` alone does not do this.
sidecars: ensure-submodules
	@echo "=== Building + copying sidecar binaries ==="
	chmod +x scripts/build-sidecars.sh
	./scripts/build-sidecars.sh --release

web:
	@echo "=== Building web UI ==="
	cd web/app && pnpm install --frozen-lockfile && pnpm run build
	@echo ""
	@echo ">>> Web UI built to public/. Ignore Vite's 'npm run preview' hint above --"
	@echo ">>> that serves static files only (no engine -> 'Server Connection Error')."
	@echo ">>> To view it working:  make preview   (browser)   or   make dev   (desktop app)"

# Build the web UI and serve it WITH the engine at http://localhost:8080 via the
# UI-only server -- unlike `pnpm preview`, the app can reach the engine here.
# Builds the `delta` CLI once if missing (one-time compile).
preview: ensure-submodules web
	@if [ ! -x build/delta ]; then \
		echo "=== Building delta CLI (one-time) ==="; \
		bash scripts/check-toolchain.sh && \
		cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=ON && \
		cmake --build build -j$$(sysctl -n hw.ncpu) --target delta; \
	fi
	@echo ""
	@echo ">>> Serving Delta at http://localhost:8080  (Ctrl+C to stop)"
	@echo ""
	./build/delta --server

dev:
	@TRIPLE=$$(rustc -vV 2>/dev/null | grep host | cut -d' ' -f2 || echo "aarch64-apple-darwin"); \
	if [ ! -f src-tauri/binaries/delta-server-$$TRIPLE ] || [ ! -f src-tauri/binaries/llama-server-$$TRIPLE ]; then \
		echo ""; \
		echo ">>> Sidecar binaries missing for $$TRIPLE -- building them now ('make sidecars')."; \
		echo ">>> ('make engine' builds into build/ but does not populate src-tauri/binaries/.)"; \
		echo ""; \
		$(MAKE) sidecars; \
	fi
	cd src-tauri && cargo tauri dev

run: all dev

clean:
	rm -rf build build_tauri_*
	rm -rf web/app/.svelte-kit web/app/node_modules/.vite
	@echo "Cleaned. Run make all to rebuild."
