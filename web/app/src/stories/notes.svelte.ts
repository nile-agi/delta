import { browser } from '$app/environment';

export interface Note {
	id: string;
	title: string;
	content: string;
	createdAt: number;
	updatedAt: number;
}

class NotesStore {
	notes = $state<Note[]>([]);
	activeNoteId = $state<string | null>(null);

	constructor() {
		if (browser) {
			try {
				const saved = localStorage.getItem('delta_notes');
				if (saved) this.notes = JSON.parse(saved);
			} catch (e) {
				console.warn('[Notes] Failed to load notes from localStorage', e);
			}
			try {
				const savedActive = localStorage.getItem('delta_active_note');
				if (savedActive) this.activeNoteId = savedActive;
			} catch (e) {
				console.warn('[Notes] Failed to load active note', e);
			}
		}
	}

	private persist() {
		if (!browser) return;
		try {
			localStorage.setItem('delta_notes', JSON.stringify(this.notes));
			if (this.activeNoteId) {
				localStorage.setItem('delta_active_note', this.activeNoteId);
			} else {
				localStorage.removeItem('delta_active_note');
			}
		} catch (e) {
			console.warn('[Notes] Failed to persist notes', e);
		}
	}

	get activeNote(): Note | null {
		return this.notes.find((n) => n.id === this.activeNoteId) ?? null;
	}

	createNote(title = 'Untitled Note') {
		const note: Note = {
			id: crypto.randomUUID(),
			title,
			content: '',
			createdAt: Date.now(),
			updatedAt: Date.now()
		};
		this.notes = [note, ...this.notes];
		this.activeNoteId = note.id;
		this.persist();
		return note;
	}

	updateNote(id: string, updates: Partial<Pick<Note, 'title' | 'content'>>) {
		this.notes = this.notes.map((n) =>
			n.id === id ? { ...n, ...updates, updatedAt: Date.now() } : n
		);
		this.persist();
	}

	deleteNote(id: string) {
		this.notes = this.notes.filter((n) => n.id !== id);
		if (this.activeNoteId === id) {
			this.activeNoteId = this.notes[0]?.id ?? null;
		}
		this.persist();
	}

	setActive(id: string | null) {
		this.activeNoteId = id;
		this.persist();
	}
}

export const notesStore = new NotesStore();