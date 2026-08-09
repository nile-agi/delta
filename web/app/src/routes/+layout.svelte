<script lang="ts">
	import { writable } from 'svelte/store';
	import NotesWindow from '$lib/components/app/misc/NotesWindow.svelte';
	import CalendarWindow from '$lib/components/app/misc/CalendarWindow.svelte';
	import { notesWindow } from '$lib/stores/notes-window.svelte';
	import { calendarWindow } from '$lib/stores/calendar-window.svelte';
	import { StickyNote, CalendarDays } from '@lucide/svelte';

	const notesVisible = writable(false);
	const calendarVisible = writable(false);

	function toggleNotes() {
		notesVisible.update((v) => !v);
	}
	function toggleCalendar() {
		calendarVisible.update((v) => !v);
	}
</script>

<!-- Your existing layout content -->
<!-- Floating Windows Layer -->
{#if serverReady && modelApiReady}
	<NotesWindow />
	<CalendarWindow />
{/if}

<!-- Toolbar / Dock -->
<div class="fixed bottom-4 left-1/2 z-[99999] flex -translate-x-1/2 items-center gap-2 rounded-full border border-border bg-background/90 px-4 py-2 shadow-lg backdrop-blur">
	<button
		class="flex h-9 w-9 items-center justify-center rounded-full text-muted-foreground transition-colors hover:bg-accent hover:text-foreground {notesWindow.state.open && !notesWindow.state.minimized ? 'bg-accent text-foreground' : ''}"
		onclick={() => notesWindow.toggle()}
		title="Notes"
	>
		<StickyNote class="h-4 w-4" />
	</button>
	<button
		class="flex h-9 w-9 items-center justify-center rounded-full text-muted-foreground transition-colors hover:bg-accent hover:text-foreground {calendarWindow.state.open && !calendarWindow.state.minimized ? 'bg-accent text-foreground' : ''}"
		onclick={() => calendarWindow.toggle()}
		title="Calendar"
	>
		<CalendarDays class="h-4 w-4" />
	</button>
</div>