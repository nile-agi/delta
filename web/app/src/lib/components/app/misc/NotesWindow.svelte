<script lang="ts">
	import { notesStore } from '$lib/stores/notes.svelte';
	import { notesWindow } from '$lib/stores/notes-window.svelte';
	import { Button } from '$lib/components/ui/button/index.js';
	import { Input } from '$lib/components/ui/input/index.js';
	import { Textarea } from '$lib/components/ui/textarea/index.js';
	import { ScrollArea } from '$lib/components/ui/scroll-area/index.js';
	import { Plus, Trash2, FileText } from '@lucide/svelte';
	import FloatingWindow from './FloatingWindow.svelte';

	let search = $state('');

	let filteredNotes = $derived(
		notesStore.notes.filter(
			(n) =>
				n.title.toLowerCase().includes(search.toLowerCase()) ||
				n.content.toLowerCase().includes(search.toLowerCase())
		)
	);

	function handleCreate() {
		notesStore.createNote();
	}

	function handleDelete(id: string, e: Event) {
		e.stopPropagation();
		notesStore.deleteNote(id);
	}

	function selectNote(id: string) {
		notesStore.setActive(id);
	}

	function updateTitle(e: Event) {
		const target = e.target as HTMLInputElement;
		if (notesStore.activeNoteId) {
			notesStore.updateNote(notesStore.activeNoteId, { title: target.value });
		}
	}

	function updateContent(e: Event) {
		const target = e.target as HTMLTextAreaElement;
		if (notesStore.activeNoteId) {
			notesStore.updateNote(notesStore.activeNoteId, { content: target.value });
		}
	}
</script>

<FloatingWindow title="Notes" store={notesWindow}>
	<div class="flex h-full w-full">
		<!-- Notes List -->
		<div class="flex w-72 flex-col border-r bg-background">
			<div class="flex items-center justify-between border-b p-4">
				<h2 class="text-lg font-semibold">Notes</h2>
				<Button size="icon" variant="ghost" onclick={handleCreate}>
					<Plus class="h-4 w-4" />
				</Button>
			</div>
			<div class="p-3">
				<Input placeholder="Search notes..." bind:value={search} class="h-8" />
			</div>
			<ScrollArea class="flex-1">
				<div class="flex flex-col gap-1 p-2">
					{#each filteredNotes as note (note.id)}
						<div
							role="button"
							tabindex="0"
							class="group flex items-start gap-2 rounded-md px-3 py-2 text-left text-sm transition-colors cursor-pointer {notesStore.activeNoteId === note.id ? 'bg-accent text-accent-foreground' : 'hover:bg-muted'}"
							onclick={() => selectNote(note.id)}
							onkeydown={(e) => {
								if (e.key === 'Enter' || e.key === ' ') {
									e.preventDefault();
									selectNote(note.id);
								}
							}}
						>
							<FileText class="mt-0.5 h-4 w-4 shrink-0 opacity-50" />
							<div class="flex-1 overflow-hidden">
								<div class="truncate font-medium">{note.title || 'Untitled'}</div>
								<div class="truncate text-xs text-muted-foreground">
									{new Date(note.updatedAt).toLocaleDateString()}
								</div>
							</div>
							<button
								class="h-6 w-6 rounded opacity-0 transition-opacity hover:bg-destructive/10 group-hover:opacity-100 focus:opacity-100 flex items-center justify-center"
								onclick={(e: Event) => handleDelete(note.id, e)}
								title="Delete note"
							>
								<Trash2 class="h-3 w-3 text-destructive" />
							</button>
						</div>
					{:else}
						<div class="px-3 py-8 text-center text-sm text-muted-foreground">
							No notes yet. Click + to create one.
						</div>
					{/each}
				</div>
			</ScrollArea>
		</div>

		<!-- Editor -->
		<div class="flex flex-1 flex-col">
			{#if notesStore.activeNote}
				<div class="border-b px-6 py-4">
					<Input
						value={notesStore.activeNote.title}
						oninput={updateTitle}
						class="border-0 bg-transparent text-xl font-semibold shadow-none focus-visible:ring-0 px-0"
						placeholder="Note title..."
					/>
				</div>
				<div class="flex-1 p-6">
					<Textarea
						value={notesStore.activeNote.content}
						oninput={updateContent}
						class="h-full resize-none border-0 bg-transparent shadow-none focus-visible:ring-0 px-0 text-base leading-relaxed"
						placeholder="Start typing your note..."
					></Textarea>
				</div>
			{:else}
				<div class="flex flex-1 items-center justify-center text-muted-foreground">
					<div class="text-center">
						<FileText class="mx-auto mb-3 h-10 w-10 opacity-20" />
						<p>Select a note or create a new one</p>
						<Button class="mt-4" onclick={handleCreate}>
							<Plus class="mr-2 h-4 w-4" />
							New Note
						</Button>
					</div>
				</div>
			{/if}
		</div>
	</div>
</FloatingWindow>