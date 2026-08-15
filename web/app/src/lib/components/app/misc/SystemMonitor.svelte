<script lang="ts">
	import FloatingWindow from './FloatingWindow.svelte';
	import { monitorWindow } from '$lib/stores/monitor-window.svelte';
	import { invoke } from '@tauri-apps/api/core';
	import { onMount, onDestroy } from 'svelte';
	import { browser } from '$app/environment';

	interface SystemStats {
		os: { name: string; version: string };
		cpu: { model: string; arch: string; cores: number; usage: number };
		memory: { total: number; available: number; usage: number };
	}

	let stats = $state<SystemStats>({
		os: { name: 'Loading...', version: '' },
		cpu: { model: '', arch: '', cores: 0, usage: 0 },
		memory: { total: 0, available: 0, usage: 0 }
	});

	let gpuName = $state('Detecting...');
	let interval: ReturnType<typeof setInterval> | null = null;

	// SMART POLLING: Only fetch data when the window is open and not minimized
	$effect(() => {
		if (monitorWindow.state.open && !monitorWindow.state.minimized) {
			if (!interval) {
				interval = setInterval(async () => {
					try {
						stats = await invoke<SystemStats>('get_system_stats');
					} catch (e) { console.error(e); }
				}, 2000);
			}
		} else {
			if (interval) {
				clearInterval(interval);
				interval = null;
			}
		}
	});

	onMount(async () => {
		// CROSS-PLATFORM GPU DETECTION via WebGL (Works on Windows, Mac, Linux)
		if (browser) {
			const canvas = document.createElement('canvas');
			const gl = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');
			if (gl && 'getExtension' in gl) {
				const debugInfo = gl.getExtension('WEBGL_debug_renderer_info');
				if (debugInfo) {
					// @ts-ignore
					gpuName = gl.getParameter(debugInfo.UNMASKED_RENDERER_WEBGL);
				} else {
					// @ts-ignore
					gpuName = gl.getParameter(gl.RENDERER);
				}
			}
		}

		// Initial fetch
		try {
			stats = await invoke<SystemStats>('get_system_stats');
		} catch (e) {}
	});

	onDestroy(() => {
		if (interval) clearInterval(interval);
	});

	function formatBytes(bytes: number): string {
		if (bytes === 0) return '0 GB';
		return (bytes / (1024 ** 3)).toFixed(2) + ' GB';
	}
</script>

<FloatingWindow title="System Monitor" store={monitorWindow} minWidth={300} minHeight={400}>
	<div class="p-4 overflow-y-auto h-full bg-gray-900 text-gray-100 font-mono">
		<h2 class="text-lg font-bold text-blue-400 border-b pb-2 mb-4">Live Hardware</h2>
		
		<section class="mb-6">
			<h3 class="text-sm font-bold uppercase text-gray-400">1. OS</h3>
			<p class="text-sm">{stats.os.name} ({stats.os.version})</p>
		</section>

		<section class="mb-6">
			<h3 class="text-sm font-bold uppercase text-gray-400">2. CPU ({stats.cpu.usage.toFixed(1)}%)</h3>
			<p class="text-xs text-gray-400 mb-2">{stats.cpu.model} • {stats.cpu.cores} Cores • {stats.cpu.arch}</p>
			<div class="w-full bg-gray-700 rounded-full h-2.5">
				<div class="bg-blue-500 h-2.5 rounded-full transition-all" style="width: {stats.cpu.usage}%"></div>
			</div>
		</section>

		<section class="mb-6">
			<h3 class="text-sm font-bold uppercase text-gray-400">3. Memory ({stats.memory.usage.toFixed(1)}%)</h3>
			<p class="text-xs text-gray-400 mb-2">{formatBytes(stats.memory.total - stats.memory.available)} / {formatBytes(stats.memory.total)}</p>
			<div class="w-full bg-gray-700 rounded-full h-2.5">
				<div class="bg-green-500 h-2.5 rounded-full transition-all" style="width: {stats.memory.usage}%"></div>
			</div>
		</section>

		<section>
			<h3 class="text-sm font-bold uppercase text-gray-400">4. GPU Activities</h3>
			<p class="text-xs text-blue-300 break-words">{gpuName}</p>
			<p class="text-[10px] text-gray-500 mt-1">*Live VRAM tracking requires native OS drivers (NVML/Metal).* </p>
		</section>
	</div>
</FloatingWindow>