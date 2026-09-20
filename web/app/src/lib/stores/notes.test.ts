import { beforeEach, afterEach, describe, expect, it, vi } from 'vitest';

vi.mock('$app/environment', () => ({ browser: true }));

let storage: Map<string, string>;
beforeEach(() => {
	vi.resetModules();
	storage = new Map();
	vi.stubGlobal('localStorage', {
		getItem: (key: string) => storage.get(key) ?? null,
		setItem: (key: string, value: string) => storage.set(key, value),
		removeItem: (key: string) => storage.delete(key)
	});
	vi.stubGlobal('confirm', () => true);
	vi.stubGlobal('alert', () => {});
	vi.spyOn(Date, 'now').mockReturnValue(1000);
});
afterEach(() => {
	vi.restoreAllMocks();
	vi.unstubAllGlobals();
});

describe('note persistence', () => {
	it('does not advance timestamps for unchanged updates or selection', async () => {
		const { notesStore } = await import('./notes.svelte');
		const note = notesStore.createNote();
		vi.mocked(Date.now).mockReturnValue(2000);
		notesStore.updateNote(note.id, { content: '' });
		notesStore.setActive(note.id);
		expect(notesStore.activeNote?.updatedAt).toBe(1000);
	});

	it('protects identity and creation time while saving actual edits', async () => {
		const { notesStore } = await import('./notes.svelte');
		const note = notesStore.createNote();
		vi.mocked(Date.now).mockReturnValue(2000);
		notesStore.updateNote(note.id, { content: 'Changed', createdAt: 99, id: 'wrong' } as never);
		expect(notesStore.activeNote).toMatchObject({
			id: note.id,
			createdAt: 1000,
			updatedAt: 2000,
			content: 'Changed'
		});
	});

	it('retains unsaved changes and exposes failures until retry succeeds', async () => {
		const { notesStore } = await import('./notes.svelte');
		const note = notesStore.createNote();
		const write = vi.spyOn(localStorage, 'setItem').mockImplementation(() => {
			throw new Error('Quota');
		});
		notesStore.updateNote(note.id, { content: 'Keep this draft' });
		expect(notesStore.activeNote?.content).toBe('Keep this draft');
		expect(notesStore.saveError).toBeTruthy();
		write.mockRestore();
		notesStore.retrySave();
		expect(notesStore.saveError).toBeNull();
		expect(JSON.parse(storage.get('delta_notes')!)[0].content).toBe('Keep this draft');
	});

	it('clears persisted selection after deleting the last note', async () => {
		const { notesStore } = await import('./notes.svelte');
		const note = notesStore.createNote();
		notesStore.deleteNote(note.id);
		expect(storage.has('delta_active_note')).toBe(false);
	});
});
