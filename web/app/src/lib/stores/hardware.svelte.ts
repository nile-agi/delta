import { browser } from '$app/environment';
import { getModelApiBaseUrl, resolveModelApiBaseUrl } from '$lib/utils/model-api-url';

export interface GPUMetrics {
	name: string;
	vram_used_gb: number;
	vram_total_gb: number;
	gpu_util_pct: number;
	temp_c: number;
	power_w: number;
}

export interface HardwareState {
	system_ram_used_gb: number;
	system_ram_total_gb: number;
	gpus: GPUMetrics[];
	isConnected: boolean;
}

class HardwareStore {
	state = $state<HardwareState>({
		system_ram_used_gb: 0,
		system_ram_total_gb: 0,
		gpus: [],
		isConnected: false
	});

	private eventSource: EventSource | null = null;
	private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
	private connecting = false;

	constructor() {
		// NOTE: no $effect here — this class is instantiated at module scope and
		// $effect may only run during component initialization. Using it here
		// throws effect_orphan at hydration and white-screens the whole app.
		if (browser) {
			void this.connect();
		}
	}

	async connect() {
		if (!browser || this.eventSource || this.connecting) return;
		this.connecting = true;
		try {
			await resolveModelApiBaseUrl(); // same-origin probe; sync no-op under Tauri
		} catch {
			/* fall back to port+1 URL */
		}
		this.connecting = false;
		if (this.eventSource) return;

		// Re-read each attempt: under Tauri, __DELTA_MODEL_API_PORT__ is injected
		// by the Rust side once the Model API is ready, so retries self-heal.
		const url = `${getModelApiBaseUrl()}/api/v1/hardware/stream`;
		const es = new EventSource(url);
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
				this.state.gpus = data.gpus || [];
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
}

export const hardwareStore = new HardwareStore();