/**
 * Base URL for the Model Management API.
 * The model management API always runs on port 8081. The main server on 8080 is llama-server
 * and does not serve /api/models/*, so we always use 8081 for model API to avoid 404 when
 * the user selects a model. Uses the current page host so the UI works when opened from
 * another device (e.g. http://192.168.1.5:8080).
 */
const MODEL_API_PORT = 8081;

function build8081Url(): string {
	if (typeof window === 'undefined') {
		return `http://localhost:${MODEL_API_PORT}`;
	}
	const { protocol, hostname } = window.location;
	return `${protocol}//${hostname}:${MODEL_API_PORT}`;
}

/**
 * Resolves the model API base URL. No-op for compatibility; we always use 8081.
 * Call from root layout so any readiness gate can complete before rendering.
 */
export function resolveModelApiBaseUrl(): Promise<void> {
	return Promise.resolve();
}

/**
 * Returns the model API base URL (e.g. 'http://host:8081').
 * Always uses port 8081 so /api/models/use and other model endpoints are never
 * requested from llama-server on 8080 (which would return 404).
 */
export function getModelApiBaseUrl(): string {
	return build8081Url();
}
