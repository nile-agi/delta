<script lang="ts">
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
  let errorMsg = $state('');
  let interval: ReturnType<typeof setInterval> | null = null;

  async function fetchStats() {
    try {
      stats = await invoke<SystemStats>('get_system_stats');
      errorMsg = '';
    } catch (e) {
      errorMsg = String(e);
    }
  }

  onMount(() => {
    fetchStats();
    interval = setInterval(fetchStats, 2000);
    
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
  });

  onDestroy(() => {
    if (interval) clearInterval(interval);
  });

  function formatBytes(bytes: number): string {
    if (!bytes) return '0 GB';
    return (bytes / (1024 ** 3)).toFixed(2) + ' GB';
  }
</script>

<!-- ❌ REMOVED: <FloatingWindow> wrapper -->
<div class="p-4 h-screen overflow-y-auto bg-gray-900 text-gray-100 font-mono">
  <h2 class="text-lg font-bold text-blue-400 border-b pb-2 mb-4">Live Hardware</h2>
  {#if errorMsg}
    <div class="mb-4 p-2 bg-red-900/60 border border-red-500 rounded text-red-200 text-xs break-words">
      ⚠️ Backend error: {errorMsg}
    </div>
  {/if}
  <section class="mb-6">
    <h3 class="text-sm font-bold uppercase text-gray-400">1. OS</h3>
    <p class="text-sm">{stats.os.name} ({stats.os.version})</p>
  </section>
  <section class="mb-6">
    <h3 class="text-sm font-bold uppercase text-gray-400">2. CPU ({stats.cpu.usage.toFixed(1)}%)</h3>
    <p class="text-xs text-gray-400 mb-2">{stats.cpu.model} • {stats.cpu.cores} Cores • {stats.cpu.arch}</p>
    <div class="w-full bg-gray-700 rounded-full h-2.5">
      <div class="h-2.5 rounded-full transition-all {stats.cpu.usage > 80 ? 'bg-red-500' : 'bg-blue-500'}" style="width: {Math.min(100, stats.cpu.usage)}%"></div>
    </div>
  </section>
  <section class="mb-6">
    <h3 class="text-sm font-bold uppercase text-gray-400">3. Memory ({stats.memory.usage.toFixed(1)}%)</h3>
    <p class="text-xs text-gray-400 mb-2">{formatBytes(stats.memory.total - stats.memory.available)} used / {formatBytes(stats.memory.total)} total</p>
    <div class="w-full bg-gray-700 rounded-full h-2.5">
      <div class="h-2.5 rounded-full transition-all {stats.memory.usage > 80 ? 'bg-red-500' : 'bg-green-500'}" style="width: {Math.min(100, stats.memory.usage)}%"></div>
    </div>
  </section>
  <section>
    <h3 class="text-sm font-bold uppercase text-gray-400">4. GPU Activities</h3>
    <p class="text-xs text-blue-300 break-words">{gpuName}</p>
  </section>
</div>