<script lang="ts">
  import { goto } from '$app/navigation';
  import type { Note } from '../+page';

  export let data: { note: Note | null; error?: string };

  let editing = false;
  let saving = false;
  let deleting = false;

  let editTitle = '';
  let editContent = '';
  let editFolder = '';
  let editTags = '';
  let editPinned = false;

  $: if (data.note && !editing) {
    editTitle = data.note.title;
    editContent = data.note.content;
    editFolder = data.note.folder;
    editTags = data.note.tags;
    editPinned = data.note.pinned;
  }

  async function saveNote() {
    if (!data.note) return;
    saving = true;
    const res = await fetch(`/api/notes/${data.note.id}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        title: editTitle,
        content: editContent,
        folder: editFolder,
        tags: editTags,
        pinned: editPinned
      })
    });
    saving = false;
    if (res.ok) {
      editing = false;
      const refreshed = await fetch(`/api/notes/${data.note.id}`);
      if (refreshed.ok) {
        data.note = await refreshed.json();
      }
    } else {
      alert('Failed to save note');
    }
  }

  async function deleteNote() {
    if (!data.note) return;
    if (!confirm('Delete this note permanently?')) return;
    deleting = true;
    const res = await fetch(`/api/notes/${data.note.id}`, { method: 'DELETE' });
    deleting = false;
    if (res.ok) {
      goto('/notes');
    } else {
      alert('Failed to delete note');
    }
  }

  async function togglePin() {
    if (!data.note) return;
    const res = await fetch(`/api/notes/${data.note.id}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ pinned: !data.note.pinned })
    });
    if (res.ok) {
      const refreshed = await fetch(`/api/notes/${data.note.id}`);
      if (refreshed.ok) {
        data.note = await refreshed.json();
      }
    }
  }

  function formatDate(iso: string) {
    return new Date(iso).toLocaleString(undefined, {
      month: 'short',
      day: 'numeric',
      year: 'numeric',
      hour: '2-digit',
      minute: '2-digit'
    });
  }
</script>

<svelte:head>
  <title>{data.note?.title || 'Note'} — Delta</title>
</svelte:head>

<div class="max-w-3xl mx-auto p-6">
  {#if data.error || !data.note}
    <div class="text-center py-12">
      <p class="text-lg text-muted-foreground">{data.error || 'Note not found'}</p>
      <a href="/notes" class="inline-block mt-4 text-primary hover:underline">← Back to Notes</a>
    </div>
  {:else}
    <div class="flex items-center justify-between mb-4">
      <a href="/notes" class="text-sm text-muted-foreground hover:text-foreground transition">← All Notes</a>
      <div class="flex gap-2">
        <button
          class="px-3 py-1.5 text-sm border rounded-md hover:bg-accent transition"
          on:click={togglePin}
        >
          {data.note.pinned ? 'Unpin' : 'Pin'}
        </button>
        <button
          class="px-3 py-1.5 text-sm border rounded-md hover:bg-accent transition"
          on:click={() => editing = !editing}
        >
          {editing ? 'Cancel' : 'Edit'}
        </button>
        <button
          class="px-3 py-1.5 text-sm border border-destructive text-destructive rounded-md hover:bg-destructive/10 transition disabled:opacity-50"
          on:click={deleteNote}
          disabled={deleting}
        >
          {deleting ? 'Deleting...' : 'Delete'}
        </button>
      </div>
    </div>

    {#if editing}
      <div class="space-y-3">
        <input
          class="w-full text-xl font-bold px-3 py-2 border rounded-md bg-background"
          bind:value={editTitle}
        />
        <div class="flex gap-2">
          <input
            class="flex-1 px-3 py-2 border rounded-md bg-background text-sm"
            placeholder="Folder"
            bind:value={editFolder}
          />
          <input
            class="flex-1 px-3 py-2 border rounded-md bg-background text-sm"
            placeholder="Tags (comma-separated)"
            bind:value={editTags}
          />
          <label class="flex items-center gap-2 px-3 py-2 border rounded-md bg-background text-sm cursor-pointer">
            <input type="checkbox" bind:checked={editPinned} />
            Pinned
          </label>
        </div>
        <!-- CORRECT -->
        <textarea
        class="w-full px-3 py-2 border rounded-md bg-background min-h-[300px] resize-y font-mono text-sm leading-relaxed"
        bind:value={editContent}
        ></textarea>
        <div class="flex justify-end">
          <button
            class="px-6 py-2 bg-primary text-primary-foreground rounded-md font-medium hover:bg-primary/90 transition disabled:opacity-50"
            on:click={saveNote}
            disabled={saving}
          >
            {saving ? 'Saving...' : 'Save Changes'}
          </button>
        </div>
      </div>
    {:else}
      <div>
        <h1 class="text-2xl font-bold mb-2">{data.note.title}</h1>
        <div class="flex items-center gap-2 text-xs text-muted-foreground mb-4">
          {#if data.note.folder}
            <span class="bg-muted px-2 py-0.5 rounded">{data.note.folder}</span>
          {/if}
          {#if data.note.tags}
            <span>{data.note.tags}</span>
          {/if}
          {#if data.note.pinned}
            <span class="bg-primary text-primary-foreground px-2 py-0.5 rounded">Pinned</span>
          {/if}
          <span>Updated {formatDate(data.note.updated_at)}</span>
        </div>
        <div class="prose dark:prose-invert max-w-none whitespace-pre-wrap">{data.note.content}</div>
      </div>
    {/if}
  {/if}
</div>