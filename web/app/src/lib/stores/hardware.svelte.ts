// import { browser } from '$app/environment';
// import { getModelApiBaseUrl, resolveModelApiBaseUrl } from '$lib/utils/model-api-url';

// export interface GPUMetrics {
// 	name: string;
// 	vram_used_gb: number;
// 	vram_total_gb: number;
// 	gpu_util_pct: number;
// 	temp_c: number;
// 	power_w: number;
// }

// export interface HardwareState {
// 	system_ram_used_gb: number;
// 	system_ram_total_gb: number;
// 	gpus: GPUMetrics[];
// 	isConnected: boolean;
// }

// const DEFAULT_MAPI_PORT = 8081; // delta-server sidecar default (model API = server port + 1)

// async function probe(base: string): Promise<boolean> {
// 	try {
// 		const res = await fetch(`${base}/api/v1/hardware/snapshot`, { cache: 'no-store' });
// 		return res.ok;
// 	} catch {
// 		return false;
// 	}
// }

// class HardwareStore {
// 	state = $state<HardwareState>({
// 		system_ram_used_gb: 0,
// 		system_ram_total_gb: 0,
// 		gpus: [],
// 		isConnected: false
// 	});

// 	private eventSource: EventSource | null = null;
// 	private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
// 	private connecting = false;

// 	constructor() {
// 		// NOTE: no $effect here — this class is instantiated at module scope and
// 		// $effect may only run during component initialization (effect_orphan).
// 		if (browser) {
// 			// Re-resolve once the sidecar announces readiness (port injection).
// 			window.addEventListener('delta-server-ready', () => void this.connect(), { once: true });
// 			void this.connect();
// 		}
// 	}

// 	async connect() {
// 		if (!browser || this.eventSource || this.connecting) return;
// 		this.connecting = true;
// 		try {
// 			await resolveModelApiBaseUrl();
// 			let base = getModelApiBaseUrl();

// 			// Dev-mode safety net: under `make dev` the webview runs on Vite (5177)
// 			// and the port+1 heuristic yields 5178. If the injected port hasn't
// 			// arrived yet, probe the sidecar default before retrying blindly.
// 			if ((window as any).__DELTA_MODEL_API_PORT__ == null && !(await probe(base))) {
// 				const fallback = `http://127.0.0.1:${DEFAULT_MAPI_PORT}`;
// 				if (await probe(fallback)) base = fallback;
// 			}

// 			const es = new EventSource(`${base}/api/v1/hardware/stream`);
// 			this.eventSource = es;

// 			es.onopen = () => {
// 				this.state.isConnected = true;
// 				console.log('[DTL] Hardware telemetry stream connected');
// 			};

// 			es.onmessage = (event) => {
// 				try {
// 					const data = JSON.parse(event.data);
// 					this.state.system_ram_used_gb = data.system_ram_used_gb || 0;
// 					this.state.system_ram_total_gb = data.system_ram_total_gb || 0;
// 					this.state.gpus = data.gpus || [];
// 				} catch (e) {
// 					console.error('[DTL] Telemetry parse error', e);
// 				}
// 			};

// 			es.onerror = () => {
// 				this.state.isConnected = false;
// 				es.close();
// 				if (this.eventSource === es) this.eventSource = null;
// 				if (this.reconnectTimer == null) {
// 					this.reconnectTimer = setTimeout(() => {
// 						this.reconnectTimer = null;
// 						void this.connect();
// 					}, 3000);
// 				}
// 			};
// 		} finally {
// 			this.connecting = false;
// 		}
// 	}

// 	disconnect() {
// 		if (this.reconnectTimer != null) {
// 			clearTimeout(this.reconnectTimer);
// 			this.reconnectTimer = null;
// 		}
// 		if (this.eventSource) {
// 			this.eventSource.close();
// 			this.eventSource = null;
// 		}
// 		this.state.isConnected = false;
// 	}
// }

// export const hardwareStore = new HardwareStore();

import { browser } from '$app/environment';
import { getModelApiBaseUrl, resolveModelApiBaseUrl } from '$lib/utils/model-api-url';

export interface GPUMetrics {
	name: string;
	backend: string;
	vram_used_gb: number;
	vram_total_gb: number;
	gpu_util_pct: number;
	temp_c: number;
	power_w: number;
}

export interface HardwareState {
	system_ram_used_gb: number;
	system_ram_total_gb: number;
	cpu_util_pct: number; // NEW
	gpus: GPUMetrics[];
	isConnected: boolean;
}

const DEFAULT_MAPI_PORT = 8081;

async function probe(base: string): Promise<boolean> {
	try {
		const res = await fetch(`${base}/api/v1/hardware/snapshot`, { cache: 'no-store' });
		return res.ok;
	} catch { return false; }
}

class HardwareStore {
	state = $state<HardwareState>({
		system_ram_used_gb: 0,
		system_ram_total_gb: 0,
		cpu_util_pct: 0,
		gpus: [],
		isConnected: false
	});

	private eventSource: EventSource | null = null;
	private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
	private connecting = false;

	constructor() {
		if (browser) {
			window.addEventListener('delta-server-ready', () => void this.connect(), { once: true });
			void this.connect();
		}
	}

	async connect() {
		if (!browser || this.eventSource || this.connecting) return;
		this.connecting = true;
		try {
			await resolveModelApiBaseUrl();
			let base = getModelApiBaseUrl();
			if ((window as any).__DELTA_MODEL_API_PORT__ == null && !(await probe(base))) {
				const fallback = `http://127.0.0.1:${DEFAULT_MAPI_PORT}`;
				if (await probe(fallback)) base = fallback;
			}

			const es = new EventSource(`${base}/api/v1/hardware/stream`);
			this.eventSource = es;

			es.onopen = () => {
				this.state.isConnected = true;
				console.log('[DTL] Hardware telemetry stream connected');
			};

			es.onmessage = (event) => {
				try {
					const data = JSON.parse(event.data);
					this.state.system_ram_used_gb = data.system_ram_used_gb || 0;
					this.state.system_ram_total_gb = data.system_ram_total_gb || 0;
					this.state.cpu_util_pct = data.cpu_util_pct || 0;
					this.state.gpus = data.gpus || [];
				} catch (e) { console.error('[DTL] Telemetry parse error', e); }
			};

			es.onerror = () => {
				this.state.isConnected = false;
				es.close();
				if (this.eventSource === es) this.eventSource = null;
				if (this.reconnectTimer == null) {
					this.reconnectTimer = setTimeout(() => {
						this.reconnectTimer = null;
						void this.connect();
					}, 3000);
				}
			};
		} finally { this.connecting = false; }
	}

	disconnect() {
		if (this.reconnectTimer != null) { clearTimeout(this.reconnectTimer); this.reconnectTimer = null; }
		if (this.eventSource) { this.eventSource.close(); this.eventSource = null; }
		this.state.isConnected = false;
	}
}

export const hardwareStore = new HardwareStore();