import { browser } from '$app/environment';
import { getModelApiBaseUrl } from '$lib/utils/model-api-url'; // Import this

export interface GPUMetrics {
    name: string;
    vram_used_gb: number;
    vram_total_gb: number;
    gpu_util_pct: number;
    temp_c: number;
}

export interface HardwareState {
    system_ram_used_gb: number;
    gpus: GPUMetrics[];
    isConnected: boolean;
}

class HardwareStore {
    state = $state<HardwareState>({
        system_ram_used_gb: 0,
        gpus: [],
        isConnected: false
    });
    
    private eventSource: EventSource | null = null;

    constructor() {
        if (browser) {
            $effect(() => {
                this.connect();
            });
        }
    }

    connect() {
        if (this.eventSource) return;
        
        // Use the Model API base URL (handles the N+1 port logic automatically)
        const baseUrl = getModelApiBaseUrl();
        const url = `${baseUrl}/api/v1/hardware/stream`;
        
        this.eventSource = new EventSource(url);
        this.eventSource.onopen = () => {
            this.state.isConnected = true;
            console.log('[DTL] Hardware telemetry stream connected');
        };
        
        this.eventSource.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                this.state.system_ram_used_gb = data.system_ram_used_gb || 0;
                this.state.gpus = data.gpus || [];
            } catch (e) {
                console.error('[DTL] Telemetry parse error', e);
            }
        };
        
        this.eventSource.onerror = () => {
            this.state.isConnected = false;
            this.eventSource?.close();
            this.eventSource = null;
            setTimeout(() => this.connect(), 3000);
        };
    }

    disconnect() {
        if (this.eventSource) {
            this.eventSource.close();
            this.eventSource = null;
            this.state.isConnected = false;
        }
    }
}

export const hardwareStore = new HardwareStore();