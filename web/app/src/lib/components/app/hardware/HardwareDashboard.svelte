<script lang="ts">
    import { hardwareStore } from '$lib/stores/hardware.svelte';
    import * as Card from '$lib/components/ui/card';
    import { Cpu, Zap, Thermometer, MemoryStick, Activity, Server } from '@lucide/svelte';

    let { fullscreen = false } = $props();
    let primaryGPU = $derived(hardwareStore.state.gpus[0]);
    
    let vramPercent = $derived(primaryGPU && primaryGPU.vram_total_gb > 0
        ? Math.min(100, Math.max(0, (primaryGPU.vram_used_gb / primaryGPU.vram_total_gb) * 100))
        : 0);
        
    let ramPercent = $derived(hardwareStore.state.system_ram_total_gb > 0
        ? Math.min(100, Math.max(0, (hardwareStore.state.system_ram_used_gb / hardwareStore.state.system_ram_total_gb) * 100))
        : 0);
        
    let cpuPercent = $derived(hardwareStore.state.cpu_util_pct);
    
    let newNodeName = $state('');
    let newNodeEndpoint = $state('');
</script>

<Card.Root class={fullscreen ? "w-full h-full" : "w-96 shadow-2xl"}>
    <Card.Header>
        <Card.Title class="flex items-center gap-2 text-sm font-mono">
            <Activity class="h-4 w-4 text-primary" />
            DHATS Telemetry Dashboard
        </Card.Title>
    </Card.Header>
    
    <Card.Content class="space-y-4">
        <!-- CPU -->
        <div class="space-y-1.5">
            <div class="flex justify-between text-xs text-muted-foreground">
                <span class="flex items-center gap-1.5"><Cpu class="h-3 w-3" /> CPU</span>
                <span>{cpuPercent.toFixed(1)}% · {hardwareStore.state.cpu_temp_c > 0 ? hardwareStore.state.cpu_temp_c.toFixed(0) + '°C' : 'N/A'}</span>
            </div>
            <div class="w-full h-2 bg-muted rounded-full overflow-hidden">
                <div class="h-full bg-blue-500 transition-all duration-300" style="width: {cpuPercent}%"></div>
            </div>
        </div>

        <!-- RAM -->
        <div class="space-y-1.5">
            <div class="flex justify-between text-xs text-muted-foreground">
                <span class="flex items-center gap-1.5"><MemoryStick class="h-3 w-3" /> RAM</span>
                <span>{hardwareStore.state.system_ram_used_gb.toFixed(1)} / {hardwareStore.state.system_ram_total_gb.toFixed(1)} GB</span>
            </div>
            <div class="w-full h-2 bg-muted rounded-full overflow-hidden">
                <div class="h-full bg-purple-500 transition-all duration-300" style="width: {ramPercent}%"></div>
            </div>
        </div>

        <!-- System Power -->
        <div class="flex justify-between text-xs text-muted-foreground">
            <span class="flex items-center gap-1.5"><Zap class="h-3 w-3" /> System Power</span>
            <span>{hardwareStore.state.system_power_w > 0 ? hardwareStore.state.system_power_w.toFixed(1) + 'W' : 'N/A'}</span>
        </div>

        <!-- GPU -->
        {#if primaryGPU}
            <div class="pt-2 border-t border-border/30 space-y-2">
                <h4 class="text-xs font-semibold uppercase tracking-wider">GPU ({primaryGPU.backend})</h4>
                
                <div class="space-y-1.5">
                    <div class="flex justify-between text-xs text-muted-foreground">
                        <span>{primaryGPU.name}</span>
                        <span>{primaryGPU.vram_used_gb.toFixed(1)} / {primaryGPU.vram_total_gb.toFixed(1)} GB</span>
                    </div>
                    <div class="w-full h-2 bg-muted rounded-full overflow-hidden">
                        <div class="h-full bg-green-500 transition-all duration-300" style="width: {vramPercent}%"></div>
                    </div>
                </div>
                
                <div class="grid grid-cols-3 gap-2 text-xs">
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
        {/if}

        <!-- RPC Workers -->
        <div class="pt-2 border-t border-border/30 space-y-2">
            <h4 class="text-xs font-semibold uppercase tracking-wider flex items-center gap-1">
                <Server class="h-3 w-3" /> RPC Workers ({hardwareStore.state.rpc_nodes.length})
            </h4>
            
            {#each hardwareStore.state.rpc_nodes as node (node.id)}
                <div class="flex items-center justify-between text-xs bg-muted/50 rounded px-2 py-1">
                    <div>
                        <div class="font-medium">{node.name}</div>
                        <div class="text-muted-foreground text-[10px]">{node.endpoint}</div>
                    </div>
                    <div class="flex gap-1">
                        <button
                            onclick={() => hardwareStore.toggleRpcNode(node.id, !node.enabled)}
                            class="px-2 py-0.5 rounded text-[10px] {node.enabled ? 'bg-green-500/20 text-green-600' : 'bg-gray-500/20 text-gray-600'}"
                        >
                            {node.enabled ? 'ON' : 'OFF'}
                        </button>
                        <button
                            onclick={() => hardwareStore.deleteRpcNode(node.id)}
                            class="px-2 py-0.5 rounded text-[10px] bg-red-500/20 text-red-600"
                        >
                            ×
                        </button>
                    </div>
                </div>
            {:else}
                <p class="text-xs text-muted-foreground">No workers. Add one to pool VRAM.</p>
            {/each}
            
            <div class="flex gap-1">
                <input bind:value={newNodeName} placeholder="Name" class="flex-1 px-2 py-1 text-xs bg-background border border-border rounded" />
                <input bind:value={newNodeEndpoint} placeholder="192.168.1.5:50052" class="flex-1 px-2 py-1 text-xs bg-background border border-border rounded" />
                <button
                    onclick={() => { if (newNodeEndpoint) { hardwareStore.addRpcNode(newNodeName || 'worker', newNodeEndpoint); newNodeName = ''; newNodeEndpoint = ''; } }}
                    class="px-2 py-1 text-xs bg-primary text-primary-foreground rounded"
                >
                    Add
                </button>
            </div>
        </div>

        <!-- Connection Status -->
        <div class="flex items-center justify-center gap-2 pt-2 border-t border-border/30">
            <div class="w-2 h-2 rounded-full {hardwareStore.state.isConnected ? 'bg-green-500 animate-pulse' : 'bg-red-500'}"></div>
            <span class="text-[10px] uppercase tracking-wider text-muted-foreground font-semibold">
                {hardwareStore.state.isConnected ? 'Live' : 'Reconnecting...'}
            </span>
        </div>
    </Card.Content>
</Card.Root>