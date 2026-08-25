import { getServerBaseUrl } from './server-base-url';
import { invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';

let cachedBaseUrl: string = '';
let resolved = false;
let resolvePromise: Promise<void> | null = null;

function isTauri(): boolean {
	if (typeof window === 'undefined') return false;
	return window.location.protocol === 'tauri:' ||
		'__TAURI__' in window ||
		'__TAURI_INTERNALS__' in window;
}

async function getTauriPorts(): Promise<{ port: number; modelApiPort: number } | null> {
	if (!isTauri()) return null;
	try {
		// get_server_status returns (port, model_api_port, ready, error)
		const status = await invoke<[number, number, boolean, boolean]>('get_server_status');
		if (status && status[2]) { // status[2] is 'ready'
			return { port: status[0], modelApiPort: status[1] };
		}
	} catch (e) {
		console.warn('[Delta] Failed to invoke get_server_status', e);
	}
	return null;
}

function getModelApiPort(): number {
	if (typeof window !== 'undefined' && (window as any).__DELTA_MODEL_API_PORT__ != null) {
		return (window as any).__DELTA_MODEL_API_PORT__;
	}
	if (typeof window === 'undefined') return 8081;
	const serverPort = parseInt(window.location.port, 10);
	return isNaN(serverPort) ? 8081 : serverPort + 1;
}

function buildModelApiUrl(port?: number): string {
    const p = port ?? getModelApiPort();
	if (typeof window === 'undefined' || isTauri()) {
		return `http://127.0.0.1:${p}`;
	}
	const { protocol, hostname } = window.location;
	return `${protocol}//${hostname}:${p}`;
}

export function resolveModelApiBaseUrl(): Promise<void> {
	if (typeof window === 'undefined') {
		cachedBaseUrl = buildModelApiUrl();
		return Promise.resolve();
	}
	if (resolvePromise !== null) {
		return resolvePromise;
	}
	resolvePromise = (async () => {
        // 1. Tauri: Ask backend for ports, or wait for the ready event
        if (isTauri()) {
            const ports = await getTauriPorts();
            if (ports) {
                cachedBaseUrl = buildModelApiUrl(ports.modelApiPort);
                resolved = true;
                return;
            }
            
            // If not ready yet, wait for the backend event
            await new Promise<void>((resolve) => {
                const unlisten = listen<{ port: number; modelApiPort: number }>('delta-server-ready', (event) => {
                    cachedBaseUrl = buildModelApiUrl(event.payload.modelApiPort);
                    resolved = true;
                    unlisten.then(fn => fn());
                    resolve();
                });
                
                // 15s timeout fallback
                setTimeout(() => {
                    if (!resolved) {
                        cachedBaseUrl = buildModelApiUrl();
                        resolved = true;
                    }
                    resolve();
                }, 15000);
            });
            return;
        }

        // 2. Browser fallback (probe same-origin)
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

export function forceModelApiSeparatePort(): void {
	cachedBaseUrl = buildModelApiUrl();
	resolved = true;
}

export function resetModelApiResolution(): void {
	resolvePromise = null;
	resolved = false;
	cachedBaseUrl = '';
}

export function getModelApiBaseUrl(): string {
	if (!resolved) return buildModelApiUrl();
	return cachedBaseUrl === '' ? '' : cachedBaseUrl;
}