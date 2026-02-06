/**
 * Base URL for the Model Management API.
 * - When the app is on port 8080: the model API may be same-origin (UI-only mode:
 *   launch_ui_only_server() serves both UI and /api/models/* on 8080) or on 8081
 *   (delta-server mode: llama-server on 8080, model API on 8081). We probe
 *   /api/models/available once; if 200 we use same-origin, else 8081.
 * - When the app is on another port we use 8081.
 * Uses the current page host so the UI works from another device (e.g. http://192.168.1.5:8080).
 */
const MODEL_API_PORT = 8081;

let cachedBaseUrl: string = '';
let resolved = false;
let resolvePromise: Promise<void> | null = null;

function build8081Url(): string {
	if (typeof window === 'undefined') {
		return `http://localhost:${MODEL_API_PORT}`;
	}
	const { protocol, hostname } = window.location;
	return `${protocol}//${hostname}:${MODEL_API_PORT}`;
}

/**
 * Resolves the model API base URL. When served from port 8080, probes same-origin
 * /api/models/available; if it returns 200 we use same-origin (UI-only mode), otherwise 8081.
 * Call once before model API calls (e.g. from root layout) so getModelApiBaseUrl() is correct.
 */
export function resolveModelApiBaseUrl(): Promise<void> {
	if (typeof window === 'undefined') {
		cachedBaseUrl = build8081Url();
		return Promise.resolve();
	}
	const port = window.location.port;
	if (port !== '8080') {
		cachedBaseUrl = build8081Url();
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
				cachedBaseUrl = build8081Url();
			}
		} catch {
			// same-origin API not available (e.g. connection refused when llama-server on 8080)
			cachedBaseUrl = build8081Url();
		}
		resolved = true;
	})();
	return resolvePromise;
}

/**
 * Returns the model API base URL ('' for same-origin or 'http://host:8081').
 * When on 8080, ensure resolveModelApiBaseUrl() has been awaited first.
 */
export function getModelApiBaseUrl(): string {
	if (typeof window === 'undefined') {
		return build8081Url();
	}
	const port = window.location.port;
	if (port !== '8080') {
		return build8081Url();
	}
	// When on 8080: use 8081 until probe has completed; then use same-origin ('') or 8081
	if (!resolved) return build8081Url();
	return cachedBaseUrl === '' ? '' : cachedBaseUrl;
}
