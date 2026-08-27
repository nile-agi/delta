<script lang="ts">
	import { hardwareStore } from '$lib/stores/hardware.svelte';
	import { Activity, Cpu, MemoryStick, Server, Thermometer, Zap, Plus, X, Pin, Shield } from '@lucide/svelte';

	let { fullscreen = false } = $props();

	const s = $derived(hardwareStore.state);
	let gpu = $derived(s.gpus[0]);

	let cpuPct = $derived(Math.min(100, Math.max(0, s.cpu_util_pct)));
	let ramPct = $derived(s.system_ram_total_gb > 0 ? (s.system_ram_used_gb / s.system_ram_total_gb) * 100 : 0);
	let vramPct = $derived(gpu && gpu.vram_total_gb > 0 ? (gpu.vram_used_gb / gpu.vram_total_gb) * 100 : 0);

	const fmt = (v: number, d = 1) => (v > 0 ? v.toFixed(d) : 'N/A');

	let nodeName = $state('');
	let nodeEndpoint = $state('');

	let pinned = $state(false);

	async function togglePin() {
		try {
			const { getCurrentWindow } = await import('@tauri-apps/api/window');
			pinned = !pinned;
			await getCurrentWindow().setAlwaysOnTop(pinned);
		} catch {
			/* not a Tauri window */
		}
	}

	function addNode() {
		if (!nodeEndpoint.trim()) return;
		hardwareStore.addRpcNode(nodeName.trim() || 'worker', nodeEndpoint.trim());
		nodeName = '';
		nodeEndpoint = '';
	}
</script>

<div class="flex h-full w-full flex-col gap-3 overflow-y-auto p-4 text-sm">
	<!-- Header with Pin button -->
	<div class="flex items-center justify-between">
		<h2 class="flex items-center gap-2 font-mono text-base font-semibold">
			<Activity class="h-4 w-4 text-primary" />
			DHATS Telemetry
		</h2>
		<div class="flex items-center gap-2">
			<!-- Pin button: only visible in fullscreen (Tauri window) mode -->
			{#if fullscreen}
				<button
					onclick={togglePin}
					title={pinned ? 'Unpin (allow behind other windows)' : 'Keep on top'}
					class="rounded p-1 text-muted-foreground transition-colors hover:bg-muted hover:text-foreground"
				>
					<Pin class="h-3.5 w-3.5 {pinned ? 'fill-primary text-primary' : ''}" />
				</button>
			{/if}
			<!-- Live status badge -->
			<span class="flex items-center gap-1.5 text-[10px] font-semibold uppercase tracking-wider text-muted-foreground">
				<span class="h-2 w-2 rounded-full {s.isConnected ? 'animate-pulse bg-green-500' : 'bg-red-500'}"></span>
				{s.isConnected ? 'Live' : 'Reconnecting…'}
			</span>
		</div>
	</div>

    <!-- DHATS Brain: Resource availability -->
	<div class="rounded-lg border border-border/40 bg-muted/30 p-3">
		<div class="mb-2 text-xs font-semibold uppercase tracking-wider text-muted-foreground">
			Available Resources
		</div>
		<div class="grid grid-cols-2 gap-2 text-xs">
			<div class="flex items-center justify-between">
				<span class="flex items-center gap-1 text-muted-foreground">
					<Zap class="h-3 w-3" /> GPU VRAM
				</span>
				<span class="font-mono font-semibold text-green-600">
					{s.gpu_available_gb.toFixed(1)} GB
				</span>
			</div>
			<div class="flex items-center justify-between">
				<span class="flex items-center gap-1 text-muted-foreground">
					<MemoryStick class="h-3 w-3" /> RAM
				</span>
				<span class="font-mono font-semibold text-blue-600">
					{s.ram_available_gb.toFixed(1)} GB
				</span>
			</div>
		</div>
		{#if s.heal_recoveries > 0}
			<div class="mt-2 rounded border border-amber-500/40 bg-amber-500/10 px-2 py-1.5 text-[10px] text-amber-600">
				🛡️ {s.heal_reason || 'Auto-recovered from memory issues'}
			</div>
		{/if}
	</div>

	<!-- DHATS Brain: Self-heal status banner -->
	{#if s.heal_recoveries > 0}
		<div class="rounded-lg border border-amber-500/40 bg-amber-500/10 px-3 py-2 text-xs text-amber-600 dark:text-amber-500">
			<div class="flex items-center gap-2">
				<Shield class="h-4 w-4 shrink-0" />
				<div class="min-w-0">
					<div class="font-semibold">Delta auto-recovered {s.heal_recoveries}×</div>
					<div class="text-[10px] opacity-90">
						{s.heal_reason || 'GPU out-of-memory'} — GPU layers now {s.active_ngl >= 0 ? s.active_ngl : 'all'}
					</div>
				</div>
			</div>
		</div>
	{/if}

	<!-- CPU + RAM (2 columns only when there's room) -->
	<div class="grid gap-3 {fullscreen ? 'sm:grid-cols-2' : 'grid-cols-1'}">
		<div class="rounded-lg border border-border/40 bg-muted/30 p-3">
			<div class="mb-2 flex items-center justify-between text-xs text-muted-foreground">
				<span class="flex items-center gap-1.5"><Cpu class="h-3.5 w-3.5" /> CPU</span>
				<span class="font-mono text-foreground">
					{cpuPct.toFixed(0)}% · {fmt(s.cpu_temp_c, 0)}{s.cpu_temp_c > 0 ? '°C' : ''}
				</span>
			</div>
			<div class="h-2 w-full overflow-hidden rounded-full bg-muted">
				<div class="h-full rounded-full bg-blue-500 transition-all duration-200 ease-linear" style="width:{cpuPct}%"></div>
			</div>
		</div>

		<div class="rounded-lg border border-border/40 bg-muted/30 p-3">
			<div class="mb-2 flex items-center justify-between text-xs text-muted-foreground">
				<span class="flex items-center gap-1.5"><MemoryStick class="h-3.5 w-3.5" /> RAM</span>
				<span class="font-mono text-foreground">
					{s.system_ram_used_gb.toFixed(1)} / {s.system_ram_total_gb.toFixed(1)} GB
				</span>
			</div>
			<div class="h-2 w-full overflow-hidden rounded-full bg-muted">
				<div class="h-full rounded-full bg-purple-500 transition-all duration-200 ease-linear" style="width:{ramPct}%"></div>
			</div>
		</div>
	</div>

	<!-- GPU -->
	{#if gpu}
		<div class="rounded-lg border border-border/40 bg-muted/30 p-3">
			<div class="mb-2 flex items-center justify-between text-xs text-muted-foreground">
				<span class="flex items-center gap-1.5"><Zap class="h-3.5 w-3.5" /> GPU · {gpu.backend}</span>
				<span class="font-mono text-foreground">{gpu.vram_used_gb.toFixed(1)} / {gpu.vram_total_gb.toFixed(1)} GB</span>
			</div>
			<div class="mb-3 h-2 w-full overflow-hidden rounded-full bg-muted">
				<div class="h-full rounded-full bg-green-500 transition-all duration-200 ease-linear" style="width:{vramPct}%"></div>
			</div>
			<div class="grid grid-cols-3 gap-2 text-center text-xs">
				<div class="rounded-md bg-background/60 py-1.5">
					<Thermometer class="mx-auto mb-0.5 h-3.5 w-3.5 text-orange-500" />
					<span class="font-mono">{fmt(gpu.temp_c, 0)}{gpu.temp_c > 0 ? '°C' : ''}</span>
				</div>
				<div class="rounded-md bg-background/60 py-1.5">
					<Activity class="mx-auto mb-0.5 h-3.5 w-3.5 text-yellow-500" />
					<span class="font-mono">{gpu.gpu_util_pct > 0 ? gpu.gpu_util_pct + '%' : 'N/A'}</span>
				</div>
				<div class="rounded-md bg-background/60 py-1.5">
					<Zap class="mx-auto mb-0.5 h-3.5 w-3.5 text-red-500" />
					<span class="font-mono">{fmt(gpu.power_w, 0)}{gpu.power_w > 0 ? 'W' : ''}</span>
				</div>
			</div>
			<div class="mt-2 truncate text-right text-[10px] text-muted-foreground">{gpu.name}</div>
		</div>
	{:else}
		<div class="rounded-lg border border-dashed border-border/60 p-4 text-center text-xs text-muted-foreground">
			No dedicated GPU detected — running on CPU.
		</div>
	{/if}

	<!-- DHATS Brain: GPU Memory Budget -->
	{#if s.gpu_budget_gb > 0 || s.gpus.length}
		<div class="flex items-center justify-between rounded-lg border border-border/40 bg-muted/30 px-3 py-2 text-xs text-muted-foreground">
			<span class="flex items-center gap-1.5"><Shield class="h-3.5 w-3.5" /> Available VRAM (live)</span>
			<span class="font-mono text-foreground">
				{s.gpu_budget_gb.toFixed(1)} GB
				{#if s.active_ngl >= 0}<span class="ml-1 text-[10px] opacity-70">({s.active_ngl} layers)</span>{/if}
			</span>
		</div>
	{/if}

	<!-- System power -->
	<div class="flex items-center justify-between rounded-lg border border-border/40 bg-muted/30 px-3 py-2 text-xs text-muted-foreground">
		<span class="flex items-center gap-1.5"><Zap class="h-3.5 w-3.5" /> System power</span>
		<span class="font-mono text-foreground">{fmt(s.system_power_w)}{s.system_power_w > 0 ? 'W' : ''}</span>
	</div>

	<!-- RPC workers -->
	<div class="rounded-lg border border-border/40 bg-muted/30 p-3">
		<div class="mb-2 flex items-center justify-between text-xs text-muted-foreground">
			<span class="flex items-center gap-1.5"><Server class="h-3.5 w-3.5" /> RPC workers</span>
			<span class="font-mono">{s.rpc_nodes.length}</span>
		</div>

		{#if s.rpc_nodes.length}
			<ul class="mb-3 space-y-1.5">
				{#each s.rpc_nodes as node (node.id)}
					<li class="flex items-center justify-between gap-2 rounded-md bg-background/60 px-2 py-1.5 text-xs">
						<div class="min-w-0">
							<div class="truncate font-medium">{node.name}</div>
							<div class="truncate font-mono text-[10px] text-muted-foreground">{node.endpoint}</div>
						</div>
						<div class="flex shrink-0 items-center gap-1">
							<button
								class="rounded px-1.5 py-0.5 text-[10px] font-semibold {node.enabled
									? 'bg-green-500/15 text-green-600'
									: 'bg-muted text-muted-foreground'}"
								onclick={() => hardwareStore.toggleRpcNode(node.id, !node.enabled)}
							>
								{node.enabled ? 'ON' : 'OFF'}
							</button>
							<button
								class="rounded bg-red-500/15 p-0.5 text-red-600"
								onclick={() => hardwareStore.deleteRpcNode(node.id)}
								aria-label="Remove worker"
							>
								<X class="h-3 w-3" />
							</button>
						</div>
					</li>
				{/each}
			</ul>
		{:else}
			<p class="mb-3 text-xs text-muted-foreground">No workers yet — add one to pool VRAM across machines.</p>
		{/if}

		<!-- Wrapping form: never clips on narrow windows -->
		<form class="flex flex-wrap gap-1.5" onsubmit={(e) => { e.preventDefault(); addNode(); }}>
			<input
				bind:value={nodeName}
				placeholder="Name"
				class="min-w-[6rem] flex-1 rounded-md border border-border bg-background px-2 py-1.5 text-xs outline-none focus:ring-1 focus:ring-primary"
			/>
			<input
				bind:value={nodeEndpoint}
				placeholder="192.168.1.5:50052"
				class="min-w-[9rem] flex-[2] rounded-md border border-border bg-background px-2 py-1.5 font-mono text-xs outline-none focus:ring-1 focus:ring-primary"
			/>
			<button
				type="submit"
				class="flex items-center gap-1 rounded-md bg-primary px-2.5 py-1.5 text-xs font-semibold text-primary-foreground"
			>
				<Plus class="h-3 w-3" /> Add
			</button>
		</form>
	</div>
</div>