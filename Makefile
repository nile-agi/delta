.PHONY: all engine sidecars web dev run clean help submodules

help:
	@echo "make engine   - Build C++ engine"
	@echo "make sidecars - Copy binaries to src-tauri/binaries/"
	@echo "make web      - Build SvelteKit web UI"
	@echo "make dev      - Run Tauri desktop app"
	@echo "make run      - Full rebuild then run"
	@echo "make clean    - Remove build artifacts"

all: engine sidecars web

submodules:
	git submodule update --init --recursive

engine:
	@echo "=== Building engine ==="
	mkdir -p build
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=ON
	cmake --build build -j$$(sysctl -n hw.ncpu) --target delta-server --target llama-server

sidecars:
	@echo "=== Copying sidecar binaries ==="
	chmod +x scripts/build-sidecars.sh
	./scripts/build-sidecars.sh --release

web:
	@echo "=== Building web UI ==="
	cd web/app && pnpm install --frozen-lockfile && pnpm run build

dev:
	cd src-tauri && cargo tauri dev

run: all dev

clean:
	rm -rf build build_tauri_*
	rm -rf web/app/.svelte-kit web/app/node_modules/.vite
	@echo "Cleaned. Run make all to rebuild."
