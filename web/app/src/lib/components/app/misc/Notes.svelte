<script lang="ts">
	import { notesStore, type Note } from '$lib/stores/notes.svelte';
	import Input from '$lib/components/ui/input/input.svelte';
	import { Plus, Trash2, FileText, Pin, Palette, Search, FolderOpen } from '@lucide/svelte';
	import type NoteEditor from './notes/NoteEditor.svelte';

	let { fullscreen = false } = $props();
	let search = $state('');
	let selectedFolder = $state<string | null>(null);
	let colorOpen = $state(false);
	let editorRef: NoteEditor | undefined = $state();
	const editorModule = import('./notes/NoteEditor.svelte');
	const colors = ['red', 'orange', 'yellow', 'green', 'blue', 'purple', 'pink'];
	let activeNote = $derived(notesStore.activeNote);
	let folders = $derived([...new Set(notesStore.notes.map(n => n.folder).filter((f): f is string => !!f))]);
	let filteredNotes = $derived(notesStore.notes.filter(n =>
		(!selectedFolder || n.folder === selectedFolder) && `${n.title} ${n.content.replace(/<[^>]*>/g, ' ')}`.toLowerCase().includes(search.toLowerCase())
	).sort((a, b) => Number(!!b.pinned) - Number(!!a.pinned) || b.updatedAt - a.updatedAt));
	function selectNote(id: string) { editorRef?.flush(); colorOpen = false; notesStore.setActive(id); }
	function createNote() { editorRef?.flush(); notesStore.createNote(); }
	function deleteNote(id: string) { editorRef?.flush(); notesStore.deleteNote(id); }
	function timestamp(time: number) { return new Date(time).toLocaleString([], { dateStyle: 'medium', timeStyle: 'short' }); }
	function setColor(color: string | null) { if (activeNote) notesStore.updateNote(activeNote.id, { color }); colorOpen = false; }
</script>

<div class="notes-workspace" class:fullscreen>
	<aside class="folders-pane" aria-label="Note folders">
		<header><FolderOpen size={16} /><h2>Folders</h2></header>
		<div class="pane-scroll">
			<button class:active={!selectedFolder} onclick={() => selectedFolder = null}>All Notes <span>{notesStore.notes.length}</span></button>
			{#each folders as folder}<button class:active={selectedFolder === folder} onclick={() => selectedFolder = folder}>{folder}</button>{/each}
		</div>
	</aside>
	<section class="list-pane" aria-label="Notes list">
		<header><h2>Notes</h2><button class="icon-button" aria-label="New note" title="New note" onclick={createNote}><Plus size={18} /></button></header>
		<div class="search"><Search size={16} /><Input placeholder="Search notes..." bind:value={search} /></div>
		<div class="pane-scroll note-list" tabindex="0" role="region" aria-label="Scrollable notes">
			{#each filteredNotes as note (note.id)}
				<div class="note-row" class:active={note.id === activeNote?.id} style:border-left-color={note.color || 'transparent'}>
					<button class="note-select" aria-label={`Open ${note.title || 'Untitled'}`} aria-current={note.id === activeNote?.id ? 'true' : undefined} onclick={() => selectNote(note.id)}>
						<span class="note-icon">{#if note.emoji}{note.emoji}{:else}<FileText size={16} />{/if}</span>
						<span class="note-summary"><strong>{#if note.pinned}<Pin size={12} />{/if}{note.title || 'Untitled'}</strong><span>{note.content.replace(/<[^>]*>/g, ' ').slice(0, 100) || 'No content'}</span><time datetime={new Date(note.createdAt).toISOString()}>{timestamp(note.createdAt)}</time></span>
					</button>
					<button class="icon-button delete-note" aria-label={`Delete ${note.title}`} onclick={() => deleteNote(note.id)}><Trash2 size={14} /></button>
				</div>
			{:else}<p class="empty">{search || selectedFolder ? 'No matching notes' : 'No notes yet'}</p>{/each}
		</div>
	</section>
	<section class="detail-pane" aria-label="Selected note">
		{#if activeNote}
			<div class="title-bar">
				<Input value={activeNote.title} oninput={e => notesStore.updateNote(activeNote!.id, { title: e.currentTarget.value })} class="note-title-input" placeholder="Untitled Note" aria-label="Note title" />
				<div class="color-anchor"><button class="icon-button" aria-label="Note color" aria-expanded={colorOpen} onclick={() => colorOpen = !colorOpen}><Palette size={16} /></button>
					{#if colorOpen}<div class="color-picker"><button onclick={() => setColor(null)}>Default</button>{#each colors as color}<button style:color onclick={() => setColor(color)}>{color}</button>{/each}</div>{/if}
				</div>
				<button class="icon-button" aria-label={activeNote.pinned ? 'Unpin note' : 'Pin note'} aria-pressed={!!activeNote.pinned} onclick={() => notesStore.togglePin(activeNote!.id)}><Pin size={16} /></button>
			</div>
			<div class="dates"><span>Created {timestamp(activeNote.createdAt)}</span><span>Updated {timestamp(activeNote.updatedAt)}</span></div>
			{#if notesStore.saveError}<div role="alert" class="save-error">{notesStore.saveError}<button onclick={() => notesStore.retrySave()}>Retry save</button></div>{/if}
			{#await editorModule}<p class="empty">Loading editor…</p>{:then module}
				{#key activeNote.id}<module.default bind:this={editorRef} note={activeNote} />{/key}
			{:catch}<p role="alert">The editor could not load. Reopen Notes to retry.</p>{/await}
		{:else}<div class="empty"><FileText size={40} /><p>Select a note or create a new one</p><button onclick={createNote}>Create note</button></div>{/if}
	</section>
</div>

<style>
	.notes-workspace { display:flex; width:100%; height:100%; min-height:0; overflow:hidden; background:var(--background); color:var(--foreground); }
	.folders-pane, .list-pane, .detail-pane { display:flex; flex-direction:column; min-width:0; min-height:0; overflow:hidden; }
	.folders-pane { width:20%; border-right:1px solid var(--border); background:var(--muted); }
	.list-pane { width:30%; border-right:1px solid var(--border); }
	.detail-pane { flex:1; }
	header { display:flex; align-items:center; justify-content:space-between; gap:8px; min-height:48px; padding:8px 12px; border-bottom:1px solid var(--border); flex-shrink:0; }
	h2 { font-size:14px; font-weight:600; }
	button { cursor:pointer; border-radius:5px; }
	button:hover, .active { background:var(--accent); }
	button:focus-visible { outline:2px solid var(--ring); outline-offset:-2px; }
	.pane-scroll { flex:1; min-height:0; overflow:auto; overscroll-behavior:contain; padding:8px; }
	.folders-pane .pane-scroll button { display:flex; justify-content:space-between; gap:8px; width:100%; text-align:left; padding:8px; font-size:13px; }
	.search { display:flex; align-items:center; gap:6px; padding:10px; }
	.search :global(input) { min-width:0; }
	.note-row { display:flex; align-items:center; border-left:3px solid transparent; border-radius:6px; margin-bottom:4px; }
	.note-select { display:flex; align-items:flex-start; gap:8px; flex:1; min-width:0; padding:12px 6px; text-align:left; }
	.note-icon { flex-shrink:0; margin-top:3px; }
	.note-summary { display:flex; flex-direction:column; min-width:0; gap:3px; font-size:11px; color:var(--muted-foreground); }
	.note-summary strong { color:var(--foreground); font-size:13px; }
	.note-summary strong :global(svg) { display:inline; margin-right:4px; }
	.note-summary > span, .note-summary strong { overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
	.note-summary time { font-size:10px; }
	.icon-button { display:inline-flex; align-items:center; justify-content:center; width:30px; height:30px; flex-shrink:0; }
	.delete-note { opacity:0; } .note-row:hover .delete-note, .delete-note:focus-visible { opacity:1; }
	.title-bar { display:flex; align-items:center; gap:4px; padding:10px 12px 4px; }
	.title-bar :global(input) { flex:1; min-width:0; border:0; box-shadow:none; font-size:20px; font-weight:700; }
	.dates { display:flex; flex-wrap:wrap; gap:4px 16px; padding:0 12px 8px; font-size:10px; color:var(--muted-foreground); border-bottom:1px solid var(--border); }
	.color-anchor { position:relative; } .color-picker { position:absolute; top:100%; right:0; display:grid; grid-template-columns:repeat(2,1fr); width:150px; padding:8px; background:var(--popover); border:1px solid var(--border); border-radius:8px; z-index:30; font-size:12px; }
	.color-picker button { padding:6px; }
	.empty { display:flex; flex-direction:column; align-items:center; justify-content:center; gap:12px; flex:1; padding:20px; text-align:center; color:var(--muted-foreground); }
	.save-error { padding:8px 12px; font-size:12px; color:var(--destructive); } .save-error button { margin-left:8px; text-decoration:underline; }
</style>
