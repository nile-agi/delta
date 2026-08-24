<script lang="ts">
    import { hardwareStore } from '$lib/stores/hardware.svelte';
    import * as Card from '$lib/components/ui/card';
    import { Cpu, Zap, Thermometer } from '@lucide/svelte';

    // Svelte 5 syntax: use `let` and assign `$derived()` as a function
    let primaryGPU = $derived(hardwareStore.state.gpus[0]);
    let vramPercent = $derived(primaryGPU 
        ? Math.min(100, Math.max(0, (primaryGPU.vram_used_gb / primaryGPU.vram_total_gb) * 100))
        : 0);
</script>

<Card.Root class="w-80 bg-background/90 backdrop-blur-md border-border/50 shadow-2xl">
    <Card.Header>
        <Card.Title class="flex items-center gap-2 text-sm font-mono">
            <Cpu class="h-4 w-4 text-primary" />
            DTL Telemetry
        </Card.Title>
    </Card.Header>
    <Card.Content class="space-y-4">
        {#if primaryGPU}
            <div class="space-y-1.5">
                <div class="flex justify-between text-xs text-muted-foreground">
                    <span>VRAM ({primaryGPU.name})</span>
                    <span>{primaryGPU.vram_used_gb.toFixed(1)} / {primaryGPU.vram_total_gb.toFixed(1)} GB</span>
                </div>
                <!-- Custom Tailwind Progress Bar (Replaces missing shadcn component) -->
                <div class="w-full h-2 bg-muted rounded-full overflow-hidden">
                    <div 
                        class="h-full bg-primary transition-all duration-300 ease-out"
                        style="width: {vramPercent}%"
                    ></div>
                </div>
            </div>
            
            <div class="grid grid-cols-2 gap-2 text-xs">
                <div class="flex items-center gap-1.5 text-orange-500">
                    <Thermometer class="h-3 w-3" /> {primaryGPU.temp_c}°C
                </div>
                <div class="flex items-center gap-1.5 text-green-500">
                    <Zap class="h-3 w-3" /> {primaryGPU.gpu_util_pct}% Util
                </div>
            </div>
        {:else}
            <p class="text-xs text-muted-foreground text-center py-4">No GPU detected. Running on CPU.</p>
        {/if}

        <!-- Connection Status Indicator -->
        <div class="flex items-center gap-2 pt-2 border-t border-border/50">
            <div class="w-2 h-2 rounded-full {hardwareStore.state.isConnected ? 'bg-green-500 animate-pulse' : 'bg-red-500'}"></div>
            <span class="text-[10px] uppercase tracking-wider text-muted-foreground">
                {hardwareStore.state.isConnected ? 'Stream Active' : 'Reconnecting...'}
            </span>
        </div>
    </Card.Content>
</Card.Root>