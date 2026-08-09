import { browser } from '$app/environment';

export interface Note {
	id: string;
	title: string;
	content: string;
	createdAt: number;
	updatedAt: number;
}

function createNotesStore() {
	let notes = $state<Note[]>([]);
	let activeNoteId = $state<string | null>(null);

	if (browser) {
		const saved = localStorage.getItem('delta_notes');
		if (saved) {
			try {
				notes = JSON.parse(saved);
			} catch {
				notes = [];
			}
		}
		const savedActive = localStorage.getItem('delta_active_note');
		if (savedActive) activeNoteId = savedActive;
	}

	function persist() {
		if (browser) {
			localStorage.setItem('delta_notes', JSON.stringify(notes));
			if (activeNoteId) localStorage.setItem('delta_active_note', activeNoteId);
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
				updatedAt: Date.now()
			};
			notes = [note, ...notes];
			activeNoteId = note.id;
			persist();
			return note;
		},

		updateNote(id: string, updates: Partial<Pick<Note, 'title' | 'content'>>) {
			notes = notes.map(n =>
				n.id === id ? { ...n, ...updates, updatedAt: Date.now() } : n
			);
			persist();
		},

		deleteNote(id: string) {
			notes = notes.filter(n => n.id !== id);
			if (activeNoteId === id) activeNoteId = notes[0]?.id ?? null;
			persist();
		},

		setActive(id: string | null) {
			activeNoteId = id;
			persist();
		}
	};
}

export const notesStore = createNotesStore();