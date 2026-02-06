/**
 * Base URL for the Model Management API.
 * - When the app is served from port 8080 (UI-only, no model), the API is on the same origin (8080).
 * - When the app is served from port 8080 with a model, llama-server is on 8080 and the API is on 8081.
 * We probe same-origin /api/models/available once; if it succeeds we use same-origin, else 8081.
 * - When the app is served from another port, we use 8081.
 * Uses the current page host so the UI works when opened from another device (e.g. http://192.168.1.5:8080).
 */
const MODEL_API_PORT = 8081;

let cachedBaseUrl: string = '';
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
 * /api/models/available; if it returns 200 we use same-origin, otherwise we use port 8081.
 * Call this once before making API calls (e.g. from root layout) so getModelApiBaseUrl() is correct.
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
				return;
			}
		} catch {
			// same-origin API not available (e.g. connection refused)
		}
		cachedBaseUrl = build8081Url();
	})();
	return resolvePromise;
}

/**
 * Returns the model API base URL (e.g. '' for same-origin or 'http://host:8081').
 * Ensure resolveModelApiBaseUrl() has been awaited first when on port 8080.
 */
export function getModelApiBaseUrl(): string {
	if (typeof window === 'undefined') {
		return build8081Url();
	}
	const port = window.location.port;
	if (port !== '8080') {
		return build8081Url();
	}
	// When on 8080: if we've already resolved, use cached value; otherwise same-origin so first-time (UI-only) works until probe completes
	return cachedBaseUrl === '' && resolvePromise === null ? '' : cachedBaseUrl;
}
