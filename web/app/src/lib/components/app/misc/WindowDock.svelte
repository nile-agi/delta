<script lang="ts">
	import { X, Settings, StickyNote, Calendar, Activity } from '@lucide/svelte';
	import { dockStore } from '$lib/stores/dock.svelte';
	import { settingsWindow } from '$lib/stores/settings-window.svelte';
	import { notesWindow } from '$lib/stores/notes-window.svelte';
	import { calendarWindow } from '$lib/stores/calendar-window.svelte';
	import { hardwareWindow } from '$lib/stores/hardware-window.svelte';

	const windowMap = {
		settings: { store: settingsWindow, title: 'Settings', icon: Settings },
		notes: { store: notesWindow, title: 'Notes', icon: StickyNote },
		calendar: { store: calendarWindow, title: 'Calendar', icon: Calendar },
		'hardware-telemetry': { store: hardwareWindow, title: 'Hardware', icon: Activity }
	};

	const ACTION_BAR_IDS = new Set(['calendar', 'notes', 'settings']);
	let dockWindows = $derived(dockStore.windows.filter(w => !ACTION_BAR_IDS.has(w.id)));

	function handleRestore(id: string) {
		windowMap[id as keyof typeof windowMap]?.store.restore();
	}

	function handleClose(id: string) {
		windowMap[id as keyof typeof windowMap]?.store.close();
		dockStore.unregister(id);
	}
</script>

{#if dockWindows.length > 0}
	<div class="window-dock-layer">
		<div class="dock-inner">
			{#each dockWindows as win (win.id)}
				{@const cfg = windowMap[win.id as keyof typeof windowMap]}
				{@const Icon = cfg?.icon}
				<div class="dock-pill">
					<button
						type="button"
						class="dock-restore"
						onclick={() => handleRestore(win.id)}
						aria-label="Restore {cfg?.title ?? win.title}"
						title="Restore {cfg?.title ?? win.title}"
					>
						{#if Icon}
							<Icon class="h-3.5 w-3.5" />
						{/if}
						<span>{cfg?.title ?? win.title}</span>
					</button>
					<div class="dock-divider" aria-hidden="true"></div>
					<button
						type="button"
						class="dock-close"
						onclick={() => handleClose(win.id)}
						aria-label="Close {cfg?.title ?? win.title}"
						title="Close {cfg?.title ?? win.title}"
					>
						<X class="h-3 w-3" />
					</button>
				</div>
			{/each}
		</div>
	</div>
{/if}

<style>
	.window-dock-layer {
		position: fixed;
		bottom: 5.5rem;
		left: 0;
		right: 0;
		z-index: 99999;
		display: flex;
		justify-content: center;
		pointer-events: none;
	}

	.dock-inner {
		pointer-events: auto;
		display: flex;
		width: fit-content;
		flex-wrap: wrap;
		align-items: center;
		justify-content: center;
		gap: 0.5rem;
	}

	.dock-pill {
		display: flex;
		align-items: center;
		gap: 0.25rem;
		border-radius: 9999px;
		border: 1px solid color-mix(in oklch, var(--border) 30%, transparent);
		background-color: color-mix(in oklch, var(--background) 80%, transparent);
		padding: 0.25rem 0.5rem;
		box-shadow: 0 2px 8px rgba(0, 0, 0, 0.2);
		backdrop-filter: blur(12px);
	}

	.dock-restore {
		display: flex;
		height: 1.5rem;
		align-items: center;
		gap: 0.375rem;
		border-radius: 9999px;
		padding: 0 0.5rem;
		font-size: 0.75rem;
		font-weight: 500;
		color: var(--muted-foreground);
		background: transparent;
		border: none;
		cursor: pointer;
		transition: background-color 0.15s, color 0.15s;
	}

	.dock-restore:hover {
		background-color: var(--accent);
		color: var(--accent-foreground);
	}

	.dock-divider {
		height: 0.75rem;
		width: 1px;
		background-color: color-mix(in oklch, var(--border) 50%, transparent);
	}

	.dock-close {
		display: flex;
		height: 1.25rem;
		width: 1.25rem;
		align-items: center;
		justify-content: center;
		border-radius: 9999px;
		color: var(--muted-foreground);
		background: transparent;
		border: none;
		cursor: pointer;
		transition: background-color 0.15s, color 0.15s;
	}

	.dock-close:hover {
		background-color: var(--accent);
		color: var(--accent-foreground);
	}
</style>