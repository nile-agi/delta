<script lang="ts">
  import { goto } from '$app/navigation';
  import { page } from '$app/stores';
  import type { Note } from './+page';

  export let data: { notes: Note[]; search: string; folder: string; tags: string };

  let search = data.search || '';
  let folder = data.folder || '';
  let tags = data.tags || '';
  let creating = false;
  let newTitle = '';
  let newContent = '';
  let newFolder = 'General';

  function applyFilters() {
    const params = new URLSearchParams();
    if (search) params.set('search', search);
    if (folder) params.set('folder', folder);
    if (tags) params.set('tags', tags);
    goto(`/notes?${params.toString()}`, { replaceState: true });
  }

  async function createNote() {
    if (!newTitle.trim()) return;
    creating = true;
    const res = await fetch('/api/notes', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        title: newTitle,
        content: newContent,
        folder: newFolder
      })
    });
    if (res.ok) {
      newTitle = '';
      newContent = '';
      creating = false;
      applyFilters();
    } else {
      creating = false;
      alert('Failed to create note');
    }
  }

  function formatDate(iso: string) {
    return new Date(iso).toLocaleDateString(undefined, {
      month: 'short',
      day: 'numeric',
      hour: '2-digit',
      minute: '2-digit'
    });
  }
</script>

<svelte:head>
  <title>Notes — Delta</title>
</svelte:head>

<div class="max-w-4xl mx-auto p-6">
  <div class="flex items-center justify-between mb-6">
    <h1 class="text-2xl font-bold">Notes</h1>
    <button
      class="px-4 py-2 bg-primary text-primary-foreground rounded-md text-sm font-medium hover:bg-primary/90 transition"
      on:click={() => { newTitle = ''; newContent = ''; document.getElementById('new-note-form')?.scrollIntoView({ behavior: 'smooth' }); }}
    >
      + New Note
    </button>
  </div>

  <div class="flex flex-wrap gap-2 mb-6">
    <input
      class="flex-1 min-w-[200px] px-3 py-2 border rounded-md bg-background text-sm"
      placeholder="Search notes..."
      bind:value={search}
      on:input={applyFilters}
    />
    <input
      class="w-40 px-3 py-2 border rounded-md bg-background text-sm"
      placeholder="Folder"
      bind:value={folder}
      on:change={applyFilters}
    />
    <input
      class="w-40 px-3 py-2 border rounded-md bg-background text-sm"
      placeholder="Tags"
      bind:value={tags}
      on:change={applyFilters}
    />
  </div>

  <div id="new-note-form" class="border rounded-lg p-4 mb-6 bg-muted/30">
    <h3 class="text-sm font-semibold mb-3">Create New Note</h3>
    <input
      class="w-full px-3 py-2 border rounded-md bg-background text-sm mb-2"
      placeholder="Title"
      bind:value={newTitle}
    />
    <textarea
      class="w-full px-3 py-2 border rounded-md bg-background text-sm mb-2 min-h-[100px] resize-y"
      placeholder="Content..."
      bind:value={newContent}
    />
    <div class="flex gap-2">
      <input
        class="w-40 px-3 py-2 border rounded-md bg-background text-sm"
        placeholder="Folder"
        bind:value={newFolder}
      />
      <button
        class="px-4 py-2 bg-primary text-primary-foreground rounded-md text-sm font-medium hover:bg-primary/90 transition disabled:opacity-50"
        on:click={createNote}
        disabled={creating || !newTitle.trim()}
      >
        {creating ? 'Saving...' : 'Save Note'}
      </button>
    </div>
  </div>

  {#if data.notes.length === 0}
    <div class="text-center py-12 text-muted-foreground">
      <p class="text-lg mb-1">No notes found</p>
      <p class="text-sm">Create your first note above or ask the assistant to save one for you.</p>
    </div>
  {:else}
    <div class="grid gap-3">
      {#each data.notes as note (note.id)}
        <a
          href="/notes/{note.id}"
          class="block p-4 border rounded-lg hover:bg-accent/50 transition-colors group"
        >
          <div class="flex items-start justify-between gap-3">
            <div class="flex-1 min-w-0">
              <div class="flex items-center gap-2 mb-1">
                <h3 class="font-semibold truncate">{note.title}</h3>
                {#if note.pinned}
                  <span class="shrink-0 text-xs bg-primary text-primary-foreground px-1.5 py-0.5 rounded">
                    Pinned
                  </span>
                {/if}
              </div>
              <p class="text-sm text-muted-foreground line-clamp-2">{note.content || 'No content'}</p>
            </div>
            <span class="text-xs text-muted-foreground shrink-0 mt-1">{formatDate(note.updated_at)}</span>
          </div>
          <div class="flex items-center gap-2 mt-2 text-xs text-muted-foreground">
            {#if note.folder}
              <span class="bg-muted px-2 py-0.5 rounded">{note.folder}</span>
            {/if}
            {#if note.tags}
              <span>{note.tags}</span>
            {/if}
          </div>
        </a>
      {/each}
    </div>
  {/if}
</div>