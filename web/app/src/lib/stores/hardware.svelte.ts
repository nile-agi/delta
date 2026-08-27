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

export interface RpcNode {
	id: string;
	name: string;
	endpoint: string;
	enabled: boolean;
}

export interface HardwareState {
	system_ram_used_gb: number;
	system_ram_total_gb: number;
	cpu_util_pct: number;
	cpu_temp_c: number;
	system_power_w: number;
	gpu_budget_gb: number;       // DHATS: Real GPU memory budget (Metal recommendedMaxWorkingSetSize)
	heal_recoveries: number;     // DHATS: Number of auto-recoveries from OOM
	active_ngl: number;          // DHATS: Currently active GPU layer count
	heal_reason: string;         // DHATS: Reason for last heal
	gpus: GPUMetrics[];
	rpc_nodes: RpcNode[];
	rpc_node_count: number;
	isConnected: boolean;
}

const DEFAULT_MAPI_PORT = 8081;

async function probe(base: string): Promise<boolean> {
	try {
		const res = await fetch(`${base}/api/v1/hardware/snapshot`, { cache: 'no-store' });
		return res.ok;
	} catch {
		return false;
	}
}

class HardwareStore {
	state = $state<HardwareState>({
		system_ram_used_gb: 0,
		system_ram_total_gb: 0,
		cpu_util_pct: 0,
		cpu_temp_c: 0,
		system_power_w: 0,
		gpu_budget_gb: 0,
		heal_recoveries: 0,
		active_ngl: -1,
		heal_reason: '',
		gpus: [],
		rpc_nodes: [],
		rpc_node_count: 0,
		isConnected: false
	});

	private eventSource: EventSource | null = null;
	private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
	private connecting = false;

	constructor() {
		if (browser) {
			window.addEventListener('delta-server-ready', () => void this.connect(), { once: true });

			// Tauri event reaches ALL webviews (main + OS telemetry window),
			// so the telemetry window gets the real port without probing.
			if ('__TAURI_INTERNALS__' in window) {
				import('@tauri-apps/api/event')
					.then(({ listen }) =>
						listen<{ port: number; modelApiPort: number }>('delta-server-ready', (e) => {
							(window as any).__DELTA_MODEL_API_PORT__ = e.payload.modelApiPort;
							void this.connect();
						})
					)
					.catch(() => {});
			}

			void this.connect();
			void this.refreshRpcNodes();
		}
	}

	async connect() {
		if (!browser || this.eventSource || this.connecting) return;
		this.connecting = true;
		try {
			await resolveModelApiBaseUrl();
			let base = getModelApiBaseUrl();

			// The OS telemetry window never receives __DELTA_MODEL_API_PORT__
			// (Rust injects it into the main webview only), so probe the
			// sidecar default port as a safety net.
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
					const d = JSON.parse(event.data);
					this.state.system_ram_used_gb = d.system_ram_used_gb || 0;
					this.state.system_ram_total_gb = d.system_ram_total_gb || 0;
					this.state.cpu_util_pct = d.cpu_util_pct || 0;
					this.state.cpu_temp_c = d.cpu_temp_c || 0;
					this.state.system_power_w = d.system_power_w || 0;
					
					// DHATS Brain: parse heal status and GPU budget
					this.state.gpu_budget_gb = d.gpu_budget_gb || 0;
					this.state.heal_recoveries = d.heal_recoveries || 0;
					this.state.active_ngl = d.active_ngl ?? -1;
					this.state.heal_reason = d.heal_reason || '';
					
					this.state.rpc_node_count = d.rpc_node_count || 0;
					this.state.gpus = d.gpus || [];
				} catch (e) {
					console.error('[DTL] Telemetry parse error', e);
				}
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
		} finally {
			this.connecting = false;
		}
	}

	disconnect() {
		if (this.reconnectTimer != null) {
			clearTimeout(this.reconnectTimer);
			this.reconnectTimer = null;
		}
		if (this.eventSource) {
			this.eventSource.close();
			this.eventSource = null;
		}
		this.state.isConnected = false;
	}

	// ---- RPC worker management ----
	private base(): string {
		return getModelApiBaseUrl();
	}

	async refreshRpcNodes() {
		try {
			const res = await fetch(`${this.base()}/api/v1/rpc/nodes`, { cache: 'no-store' });
			if (res.ok) {
				const data = await res.json();
				this.state.rpc_nodes = data.nodes || [];
				this.state.rpc_node_count = this.state.rpc_nodes.filter((n: RpcNode) => n.enabled).length;
			}
		} catch {
			/* ignore */
		}
	}

	async addRpcNode(name: string, endpoint: string) {
		await fetch(`${this.base()}/api/v1/rpc/nodes`, {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify({ name, endpoint })
		});
		await this.refreshRpcNodes();
	}

	async deleteRpcNode(id: string) {
		await fetch(`${this.base()}/api/v1/rpc/nodes/${id}`, { method: 'DELETE' });
		await this.refreshRpcNodes();
	}

	async toggleRpcNode(id: string, enabled: boolean) {
		await fetch(`${this.base()}/api/v1/rpc/nodes/${id}/toggle`, {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify({ enabled })
		});
		await this.refreshRpcNodes();
	}
}

export const hardwareStore = new HardwareStore();