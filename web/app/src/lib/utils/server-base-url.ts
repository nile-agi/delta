export function getServerBaseUrl(): string {
	if (typeof window !== 'undefined' && (window as any).__DELTA_PORT__ != null) {
		return `http://127.0.0.1:${(window as any).__DELTA_PORT__}`;
	}
	return '';
}
