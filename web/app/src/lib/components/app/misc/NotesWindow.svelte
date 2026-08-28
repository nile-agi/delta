<!-- NotesWindow.svelte -->
<script lang="ts">
	import Notes from './Notes.svelte';
	import FloatingWindow from './FloatingWindow.svelte';
	import { notesWindow } from '$lib/stores/notes-window.svelte';
</script>

<FloatingWindow title="Notes" store={notesWindow}>
	<Notes />
</FloatingWindow>