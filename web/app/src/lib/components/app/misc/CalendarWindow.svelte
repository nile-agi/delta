<!-- CalendarWindow.svelte -->
<script lang="ts">
	import Calendar from './Calendar.svelte';
	import FloatingWindow from './FloatingWindow.svelte';
	import { calendarWindow } from '$lib/stores/calendar-window.svelte';
	import { browser } from '$app/environment';

	// Detect if we're in a standalone OS window
	const IS_TAURI_ENV = browser && typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;
	let isStandaloneWindow = $state(false);

	if (browser) {
		// Check URL parameter first
		const urlParams = new URLSearchParams(window.location.search);
		if (urlParams.get('window') === 'calendar') {
			isStandaloneWindow = true;
		}

		// Also check Tauri window label (more reliable)
		if (IS_TAURI_ENV) {
			import('@tauri-apps/api/window')
				.then(({ getCurrentWindow }) => {
					const label = getCurrentWindow().label;
					if (label === 'calendar') {
						isStandaloneWindow = true;
						// In standalone mode, ensure the window is open and not minimized
						calendarWindow.state.open = true;
						calendarWindow.state.minimized = false;
					}
				})
				.catch(() => {});
		}
	}
</script>

{#if isStandaloneWindow}
	<!-- Standalone OS window: render Calendar directly, fullscreen -->
	<div class="h-screen w-screen overflow-hidden bg-background">
		<Calendar fullscreen />
	</div>
{:else}
	<!-- Main app: render Calendar inside FloatingWindow -->
	<FloatingWindow title="Calendar" store={calendarWindow}>
		<Calendar />
	</FloatingWindow>
{/if}