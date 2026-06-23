/**
 * Returns the base URL for the llama-server API.
 *
 * When running inside the Tauri desktop app, the WebView origin is not the
 * same as the HTTP server, so we need absolute URLs.  In the normal browser
 * flow the page is served by the same server, so an empty string (relative
 * URLs) works fine.
 */

let cached: string | undefined;

function isTauri(): boolean {
	return typeof window !== 'undefined' && '__TAURI__' in window;
}

export function getServerBaseUrl(): string {
	if (cached !== undefined) return cached;

	if (isTauri()) {
		cached = 'http://localhost:8080';
	} else {
		cached = '.';
	}
	return cached;
}
