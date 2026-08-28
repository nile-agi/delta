<!-- CalendarWindow.svelte -->
<script lang="ts">
	import Calendar from './Calendar.svelte';
	import FloatingWindow from './FloatingWindow.svelte';
	import { calendarWindow } from '$lib/stores/calendar-window.svelte';
</script>

<FloatingWindow title="Calendar" store={calendarWindow}>
	<Calendar />
</FloatingWindow>