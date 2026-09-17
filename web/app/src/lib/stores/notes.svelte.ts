import { browser } from '$app/environment';

export interface Note {
	id: string;
	title: string;
	content: string;
	createdAt: number;
	updatedAt: number;
	pinned?: boolean;
	color?: string | null;
	emoji?: string | null;
	tags?: string[];
}

export type NoteUpdates = Partial<
	Pick<Note, 'title' | 'content' | 'pinned' | 'color' | 'emoji' | 'tags'>
>;

function createNotesStore() {
	let notes = $state<Note[]>([]);
	let activeNoteId = $state<string | null>(null);
	let saveError = $state<string | null>(null);

	if (browser) {
		const saved = localStorage.getItem('delta_notes');
		if (saved) {
			try {
				const parsed = JSON.parse(saved);
				notes = parsed.map((n: any) => ({
					...n,
					pinned: n.pinned ?? false,
					color: n.color ?? null,
					emoji: n.emoji ?? null,
					tags: n.tags ?? []
				}));
			} catch {
				notes = [];
			}
		}
		const savedActive = localStorage.getItem('delta_active_note');
		if (savedActive) activeNoteId = savedActive;
	}

	function persist() {
		if (!browser) return true;
		try {
			localStorage.setItem('delta_notes', JSON.stringify(notes));
			if (activeNoteId) localStorage.setItem('delta_active_note', activeNoteId);
			else localStorage.removeItem('delta_active_note');
			saveError = null;
			return true;
		} catch (e) {
			saveError =
				'Unable to save notes. Your changes are kept in this window. Free storage and retry.';
			return false;
		}
	}

	return {
		get notes() {
			return notes;
		},
		get saveError() {
			return saveError;
		},
		retrySave: persist,
		get activeNoteId() {
			return activeNoteId;
		},
		get activeNote() {
			return notes.find((n) => n.id === activeNoteId) ?? null;
		},

		createNote(title = 'Untitled Note') {
			const note: Note = {
				id: crypto.randomUUID(),
				title,
				content: '',
				createdAt: Date.now(),
				updatedAt: Date.now(),
				pinned: false,
				color: null,
				emoji: null,
				tags: []
			};
			notes = [note, ...notes];
			activeNoteId = note.id;
			persist();
			return note;
		},

		updateNote(id: string, updates: NoteUpdates) {
			const note = notes.find((n) => n.id === id);
			if (!note) return false;
			const allowed = ['title', 'content', 'pinned', 'color', 'emoji', 'tags'] as const;
			const changed = Object.fromEntries(
				allowed
					.filter(
						(key) => key in updates && JSON.stringify(note[key]) !== JSON.stringify(updates[key])
					)
					.map((key) => [key, updates[key]])
			);
			if (Object.keys(changed).length === 0) return saveError ? persist() : true;
			notes = notes.map((n) => (n.id === id ? { ...n, ...changed, updatedAt: Date.now() } : n));
			return persist();
		},

		deleteNote(id: string) {
			if (!confirm('Delete this note permanently?')) return;
			notes = notes.filter((n) => n.id !== id);
			if (activeNoteId === id) activeNoteId = notes[0]?.id ?? null;
			persist();
		},

		setActive(id: string | null) {
			activeNoteId = id;
			persist();
		},

		togglePin(id: string) {
			const note = notes.find((n) => n.id === id);
			if (note) {
				this.updateNote(id, { pinned: !note.pinned });
			}
		}
	};
}

export const notesStore = createNotesStore();
