<script lang="ts">
    import { hardwareStore } from '$lib/stores/hardware.svelte';
    import * as Card from '$lib/components/ui/card';
    import { Cpu, Zap, Thermometer, MemoryStick, Activity } from '@lucide/svelte';

    let primaryGPU = $derived(hardwareStore.state.gpus[0]);
    
    let vramPercent = $derived(primaryGPU && primaryGPU.vram_total_gb > 0
        ? Math.min(100, Math.max(0, (primaryGPU.vram_used_gb / primaryGPU.vram_total_gb) * 100))
        : 0);
        
    let ramPercent = $derived(hardwareStore.state.system_ram_total_gb > 0
        ? Math.min(100, Math.max(0, (hardwareStore.state.system_ram_used_gb / hardwareStore.state.system_ram_total_gb) * 100))
        : 0);
        
    let cpuPercent = $derived(hardwareStore.state.cpu_util_pct);
</script>

<Card.Root class="w-96 bg-background/95 backdrop-blur-md border-border/50 shadow-2xl">
    <Card.Header>
        <Card.Title class="flex items-center gap-2 text-sm font-mono">
            <Activity class="h-4 w-4 text-primary" />
            DHATS Telemetry Dashboard
        </Card.Title>
    </Card.Header>
    
    <Card.Content class="space-y-5">
        <!-- CPU Gauge -->
        <div class="space-y-1.5">
            <div class="flex justify-between text-xs text-muted-foreground">
                <span class="flex items-center gap-1.5"><Cpu class="h-3 w-3" /> CPU Utilization</span>
                <span>{cpuPercent.toFixed(1)}%</span>
            </div>
            <div class="w-full h-2 bg-muted rounded-full overflow-hidden">
                <div 
                    class="h-full bg-blue-500 transition-all duration-300 ease-out"
                    style="width: {cpuPercent}%"
                ></div>
            </div>
        </div>

        <!-- System RAM Gauge -->
        <div class="space-y-1.5">
            <div class="flex justify-between text-xs text-muted-foreground">
                <span class="flex items-center gap-1.5"><MemoryStick class="h-3 w-3" /> System RAM</span>
                <span>{hardwareStore.state.system_ram_used_gb.toFixed(1)} / {hardwareStore.state.system_ram_total_gb.toFixed(1)} GB</span>
            </div>
            <div class="w-full h-2 bg-muted rounded-full overflow-hidden">
                <div 
                    class="h-full bg-purple-500 transition-all duration-300 ease-out"
                    style="width: {ramPercent}%"
                ></div>
            </div>
        </div>

        <!-- GPU Section -->
        {#if primaryGPU}
            <div class="pt-2 border-t border-border/30 space-y-3">
                <h4 class="text-xs font-semibold text-foreground/80 uppercase tracking-wider">
                    GPU ({primaryGPU.backend})
                </h4>
                
                <div class="space-y-1.5">
                    <div class="flex justify-between text-xs text-muted-foreground">
                        <span>VRAM ({primaryGPU.name})</span>
                        <span>{primaryGPU.vram_used_gb.toFixed(1)} / {primaryGPU.vram_total_gb.toFixed(1)} GB</span>
                    </div>
                    <div class="w-full h-2 bg-muted rounded-full overflow-hidden">
                        <div 
                            class="h-full bg-green-500 transition-all duration-300 ease-out"
                            style="width: {vramPercent}%"
                        ></div>
                    </div>
                </div>
                
                <div class="grid grid-cols-3 gap-2 text-xs pt-1">
                    <div class="flex flex-col items-center gap-1 text-orange-500">
                        <Thermometer class="h-4 w-4" />
                        <span class="font-mono">{primaryGPU.temp_c > 0 ? primaryGPU.temp_c.toFixed(0) + '°C' : 'N/A'}</span>
                    </div>
                    <div class="flex flex-col items-center gap-1 text-yellow-500">
                        <Zap class="h-4 w-4" />
                        <span class="font-mono">{primaryGPU.gpu_util_pct > 0 ? primaryGPU.gpu_util_pct + '%' : 'N/A'}</span>
                    </div>
                    <div class="flex flex-col items-center gap-1 text-red-500">
                        <Activity class="h-4 w-4" />
                        <span class="font-mono">{primaryGPU.power_w > 0 ? primaryGPU.power_w.toFixed(0) + 'W' : 'N/A'}</span>
                    </div>
                </div>
            </div>
        {:else}
            <div class="pt-2 border-t border-border/30 text-center">
                <p class="text-xs text-muted-foreground py-2">No dedicated GPU detected. Running on CPU.</p>
            </div>
        {/if}

        <!-- Connection Status -->
        <div class="flex items-center justify-center gap-2 pt-3 border-t border-border/30">
            <div class="w-2 h-2 rounded-full {hardwareStore.state.isConnected ? 'bg-green-500 animate-pulse' : 'bg-red-500'}"></div>
            <span class="text-[10px] uppercase tracking-wider text-muted-foreground font-semibold">
                {hardwareStore.state.isConnected ? 'Live Stream Active' : 'Reconnecting...'}
            </span>
        </div>
    </Card.Content>
</Card.Root>