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

function createNotesStore() {
	let notes = $state<Note[]>([]);
	let activeNoteId = $state<string | null>(null);

	if (browser) {
		const saved = localStorage.getItem('delta_notes');
		if (saved) {
			try {
				const parsed = JSON.parse(saved);
				// Migrate old notes gracefully
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
		if (!browser) return;
		try {
			localStorage.setItem('delta_notes', JSON.stringify(notes));
			if (activeNoteId) localStorage.setItem('delta_active_note', activeNoteId);
		} catch (e) {
			// Guard against quota exceeded on large notes with embedded media
			console.error('Failed to persist notes:', e);
			alert('Storage full: try deleting old notes or removing large images.');
		}
	}

	return {
		get notes() { return notes; },
		get activeNoteId() { return activeNoteId; },
		get activeNote() { return notes.find(n => n.id === activeNoteId) ?? null; },

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

		updateNote(id: string, updates: Partial<Note>) {
			notes = notes.map(n =>
				n.id === id ? { ...n, ...updates, updatedAt: Date.now() } : n
			);
			persist();
		},

		deleteNote(id: string) {
			if (!confirm('Delete this note permanently?')) return;
			notes = notes.filter(n => n.id !== id);
			if (activeNoteId === id) activeNoteId = notes[0]?.id ?? null;
			persist();
		},

		setActive(id: string | null) {
			activeNoteId = id;
			persist();
		},

		togglePin(id: string) {
			const note = notes.find(n => n.id === id);
			if (note) {
				this.updateNote(id, { pinned: !note.pinned });
			}
		}
	};
}

export const notesStore = createNotesStore();