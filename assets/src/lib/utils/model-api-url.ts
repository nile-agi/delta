/**
 * Base URL for the Model Management API (port 8081).
 * Uses the current page host so the UI works when opened from another device (e.g. http://192.168.1.5:8080).
 */
const MODEL_API_PORT = 8081;

export function getModelApiBaseUrl(): string {
	if (typeof window === 'undefined') {
		return `http://localhost:${MODEL_API_PORT}`;
	}
	const { protocol, hostname } = window.location;
	return `${protocol}//${hostname}:${MODEL_API_PORT}`;
}
