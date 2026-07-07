export function getServerBaseUrl(): string {
	if (typeof window !== 'undefined' && (window as any).__DELTA_PORT__ != null) {
		return `http://localhost:${(window as any).__DELTA_PORT__}`;
	}
	return '';
}
