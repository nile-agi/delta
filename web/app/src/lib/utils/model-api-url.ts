/**
 * Base URL for the Model Management API.
 * The model API runs on server_port + 1 (e.g. 8080 → 8081, 8082 → 8083).
 * Probes same-origin first; if the model API is co-hosted (UI-only mode) we
 * use same-origin, otherwise we fall back to port + 1.
 */
let cachedBaseUrl: string = '';
let resolved = false;
let resolvePromise: Promise<void> | null = null;

function isTauri(): boolean {
	if (typeof window === 'undefined') return false;
	return window.location.protocol === 'tauri:' ||
		'__TAURI__' in window ||
		'__TAURI_INTERNALS__' in window;
}

function getModelApiPort(): number {
	if (typeof window === 'undefined') return 8081;
	const serverPort = parseInt(window.location.port, 10);
	return isNaN(serverPort) ? 8081 : serverPort + 1;
}

function buildModelApiUrl(): string {
	if (typeof window === 'undefined' || isTauri()) {
		return `http://localhost:${getModelApiPort()}`;
	}
	const { protocol, hostname } = window.location;
	return `${protocol}//${hostname}:${getModelApiPort()}`;
}

/**
 * Resolves the model API base URL. Probes same-origin /api/models/available;
 * if 200 we use same-origin (UI-only mode), otherwise port + 1.
 */
export function resolveModelApiBaseUrl(): Promise<void> {
	if (typeof window === 'undefined') {
		cachedBaseUrl = buildModelApiUrl();
		return Promise.resolve();
	}
	if (resolvePromise !== null) {
		return resolvePromise;
	}
	resolvePromise = (async () => {
		try {
			const res = await fetch('/api/models/available', { method: 'GET' });
			if (res.ok) {
				cachedBaseUrl = '';
			} else {
				cachedBaseUrl = buildModelApiUrl();
			}
		} catch {
			cachedBaseUrl = buildModelApiUrl();
		}
		resolved = true;
	})();
	return resolvePromise;
}

/**
 * Force model API to use port + 1 (e.g. after llama-server starts on the same port)
 */
export function forceModelApiSeparatePort(): void {
	cachedBaseUrl = buildModelApiUrl();
	resolved = true;
}

/**
 * Returns the model API base URL ('' for same-origin or 'http://host:PORT+1').
 * Ensure resolveModelApiBaseUrl() has been awaited first.
 */
export function getModelApiBaseUrl(): string {
	if (!resolved) return buildModelApiUrl();
	return cachedBaseUrl === '' ? '' : cachedBaseUrl;
}
