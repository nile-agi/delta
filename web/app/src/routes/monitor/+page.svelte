<script lang="ts">
  import { invoke } from '@tauri-apps/api/core';
  import { onMount, onDestroy } from 'svelte';

  let stats = {
    os: { name: 'Loading...', version: '' },
    cpu: { model: '', arch: '', cores: 0, usage: 0 },
    memory: { total: 0, available: 0, usage: 0 },
    gpu: { name: '', usage: 0, total_memory: 0, available_memory: 0 }
  };

  let interval: ReturnType<typeof setInterval>;

  onMount(async () => {
    async function fetchStats() {
      try {
        const data = await invoke('get_system_stats');
        stats = data as any;
      } catch (e) { console.error(e); }
    }
    await fetchStats();
    // Refresh every 2 seconds for "live" feel
    interval = setInterval(fetchStats, 2000); 
  });

  onDestroy(() => { if (interval) clearInterval(interval); });

  function formatBytes(bytes: number): string {
    if (bytes === 0) return '0 GB';
    return (bytes / (1024 ** 3)).toFixed(2) + ' GB';
  }
</script>

<div class="monitor-container">
  <h2>System Monitor</h2>

  <section class="card">
    <h3>1. Operating System</h3>
    <p><strong>Name:</strong> {stats.os.name}</p>
    <p><strong>Version:</strong> {stats.os.version}</p>
  </section>

  <section class="card">
    <h3>2. CPU</h3>
    <p><strong>Model:</strong> {stats.cpu.model}</p>
    <p><strong>Architecture:</strong> {stats.cpu.arch}</p>
    <p><strong>Cores:</strong> {stats.cpu.cores}</p>
    <div class="usage-bar-container">
      <span>Live Usage: {stats.cpu.usage.toFixed(1)}%</span>
      <div class="progress-bar">
        <div class="progress-fill" style="width: {stats.cpu.usage}%; background-color: {stats.cpu.usage > 80 ? '#ef4444' : '#3b82f6'};"></div>
      </div>
    </div>
  </section>

  <section class="card">
    <h3>3. Memory</h3>
    <p><strong>Total RAM:</strong> {formatBytes(stats.memory.total)}</p>
    <p><strong>Available RAM:</strong> {formatBytes(stats.memory.available)}</p>
    <div class="usage-bar-container">
      <span>Live Usage: {stats.memory.usage.toFixed(1)}%</span>
      <div class="progress-bar">
        <div class="progress-fill" style="width: {stats.memory.usage}%; background-color: {stats.memory.usage > 80 ? '#ef4444' : '#10b981'};"></div>
      </div>
    </div>
  </section>

  <section class="card">
    <h3>GPU Activities</h3>
    <p><em>{stats.gpu.name}</em></p>
  </section>
</div>

<style>
  .monitor-container {
    padding: 20px; font-family: system-ui, -apple-system, sans-serif;
    background: #1e1e1e; color: #e5e5e5; height: 100vh; overflow-y: auto;
  }
  h2 { border-bottom: 2px solid #3b82f6; padding-bottom: 10px; }
  .card { background: #2d2d2d; border-radius: 8px; padding: 15px; margin-bottom: 15px; }
  h3 { margin-top: 0; color: #93c5fd; }
  .progress-bar { background: #404040; border-radius: 4px; height: 20px; overflow: hidden; margin-top: 5px; }
  .progress-fill { height: 100%; transition: width 0.5s ease-in-out; }
</style>