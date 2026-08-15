<!-- web/app/src/lib/components/app/misc/SystemMonitor.svelte -->
<script lang="ts">
	import FloatingWindow from './FloatingWindow.svelte';
	import { monitorWindow } from '$lib/stores/monitor-window.svelte';
	import { invoke } from '@tauri-apps/api/core';
	import { onMount, onDestroy } from 'svelte';

	// 1. Define the exact shape of the data matching the Rust struct
	interface SystemStats {
		os: { name: string; version: string };
		cpu: { model: string; arch: string; cores: number; usage: number };
		memory: { total: number; available: number; usage: number };
		gpu: { name: string; usage: number; total_memory: number; available_memory: number };
	}

	// 2. Provide a complete dummy initial state so TS knows all properties exist
	let stats = $state<SystemStats>({
		os: { name: 'Loading...', version: '' },
		cpu: { model: '', arch: '', cores: 0, usage: 0 },
		memory: { total: 0, available: 0, usage: 0 },
		gpu: { name: 'Initializing GPU...', usage: 0, total_memory: 0, available_memory: 0 }
	});

	let interval: ReturnType<typeof setInterval>;

	onMount(async () => {
		async function fetchStats() {
			try {
				// 3. Tell invoke to type the response as SystemStats
				stats = await invoke<SystemStats>('get_system_stats');
			} catch (e) { 
				console.error(e); 
			}
		}
		await fetchStats();
		interval = setInterval(fetchStats, 2000); 
	});

	onDestroy(() => { if (interval) clearInterval(interval); });
</script>

<FloatingWindow title="System Monitor" store={monitorWindow} minWidth={300} minHeight={400}>
	<div class="p-4 overflow-y-auto h-full bg-gray-900 text-gray-100 font-mono">
		<h2 class="text-lg font-bold text-blue-400 border-b pb-2 mb-4">Live Hardware</h2>
		
		<section class="mb-6">
			<h3 class="text-sm font-bold uppercase text-gray-400">1. OS</h3>
			<p>{stats.os.name} ({stats.os.version})</p>
		</section>

		<section class="mb-6">
			<h3 class="text-sm font-bold uppercase text-gray-400">2. CPU ({stats.cpu.usage.toFixed(1)}%)</h3>
			<p class="text-xs text-gray-400 mb-1">{stats.cpu.model} • {stats.cpu.cores} Cores • {stats.cpu.arch}</p>
			<div class="w-full bg-gray-700 rounded-full h-2.5">
				<div class="bg-blue-500 h-2.5 rounded-full transition-all" style="width: {stats.cpu.usage}%"></div>
			</div>
		</section>

		<section class="mb-6">
			<h3 class="text-sm font-bold uppercase text-gray-400">3. Memory ({stats.memory.usage.toFixed(1)}%)</h3>
			<div class="w-full bg-gray-700 rounded-full h-2.5">
				<div class="bg-green-500 h-2.5 rounded-full transition-all" style="width: {stats.memory.usage}%"></div>
			</div>
		</section>

		<section>
			<h3 class="text-sm font-bold uppercase text-gray-400">GPU Activities</h3>
			<p class="text-xs">{stats.gpu.name} ({stats.gpu.usage.toFixed(1)}%)</p>
		</section>
	</div>
</FloatingWindow>