import { getServerBaseUrl } from './server-base-url';

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
	if (typeof window !== 'undefined' && (window as any).__DELTA_MODEL_API_PORT__ != null) {
		return (window as any).__DELTA_MODEL_API_PORT__;
	}
	if (typeof window === 'undefined') return 8081;
	const serverPort = parseInt(window.location.port, 10);
	return isNaN(serverPort) ? 8081 : serverPort + 1;
}

function buildModelApiUrl(): string {
	if (typeof window === 'undefined' || isTauri()) {
		return `http://127.0.0.1:${getModelApiPort()}`;
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
	if (isTauri()) {
		cachedBaseUrl = buildModelApiUrl();
		resolved = true;
		return Promise.resolve();
	}
	if (resolvePromise !== null) {
		return resolvePromise;
	}
	resolvePromise = (async () => {
		try {
			const probeBase = getServerBaseUrl();
			const res = await fetch(`${probeBase}/api/models/available`, { method: 'GET' });
			const contentType = res.headers.get('content-type') || '';
			if (res.ok && contentType.includes('application/json')) {
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
 * Reset cached resolution so resolveModelApiBaseUrl() re-probes on next call.
 */
export function resetModelApiResolution(): void {
	resolvePromise = null;
	resolved = false;
	cachedBaseUrl = '';
}

/**
 * Returns the model API base URL ('' for same-origin or 'http://host:PORT+1').
 * Ensure resolveModelApiBaseUrl() has been awaited first.
 */
export function getModelApiBaseUrl(): string {
	if (!resolved) return buildModelApiUrl();
	return cachedBaseUrl === '' ? '' : cachedBaseUrl;
}
