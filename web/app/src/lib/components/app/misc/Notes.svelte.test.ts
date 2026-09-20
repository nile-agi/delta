import { beforeEach, afterEach, expect, it, vi } from 'vitest';
import { render, cleanup } from 'vitest-browser-svelte';
import { page } from '@vitest/browser/context';
import Notes from './Notes.svelte';
import { notesStore } from '$lib/stores/notes.svelte';

function renderNotes() {
	const target = document.createElement('div');
	target.style.cssText = 'width:1100px;height:700px'; document.body.append(target);
	return render(Notes, { target });
}
beforeEach(async () => { await page.viewport(1200, 800); localStorage.removeItem('delta_notes_panes'); });
afterEach(() => {
	cleanup();
	vi.restoreAllMocks();
});

it('inserts emoji into the body after title focus without changing the note icon', async () => {
	const note = notesStore.createNote('Emoji test');
	renderNotes();
	const body = page.getByRole('textbox', { name: 'Note editor', exact: true });
	await body.fill('Hello');
	await page.getByPlaceholder('Untitled Note').click();
	await page.getByRole('button', { name: 'Add emoji', exact: true }).click();
	await page.getByRole('button', { name: '💡', exact: true }).click();
	await expect.poll(() => notesStore.notes.find((n) => n.id === note.id)?.content).toContain('💡');
	expect(notesStore.activeNote?.emoji).toBeNull();
});

it('opening legacy content does not rewrite it or update timestamps', async () => {
	const note = notesStore.createNote('Legacy');
	notesStore.updateNote(note.id, { content: 'Original plain text\nSecond line' });
	const original = { ...notesStore.activeNote! };
	renderNotes();
	await expect
		.element(page.getByRole('textbox', { name: 'Note editor', exact: true }))
		.toHaveTextContent('Original plain text');
	await new Promise((resolve) => setTimeout(resolve, 650));
	expect(notesStore.activeNote).toEqual(original);
});

it('saves edits to their original note when switching immediately', async () => {
	const other = notesStore.createNote('Other note');
	const first = notesStore.createNote('First note');
	renderNotes();
	await page
		.getByRole('textbox', { name: 'Note editor', exact: true })
		.fill('Draft belongs to first');
	await page.getByRole('button', { name: 'Open Other note', exact: true }).click();
	await expect.poll(() => notesStore.activeNoteId).toBe(other.id);
	expect(notesStore.notes.find((n) => n.id === first.id)?.content).toContain(
		'Draft belongs to first'
	);
	expect(notesStore.notes.find((n) => n.id === other.id)?.content).toBe('');
});

it('formats a selection and undoes it through toolbar commands', async () => {
	notesStore.createNote('Formatting');
	renderNotes();
	const body = page.getByRole('textbox', { name: 'Note editor', exact: true });
	await body.fill('Selected text');
	const element = body.element();
	const range = document.createRange();
	range.selectNodeContents(element);
	window.getSelection()?.removeAllRanges();
	window.getSelection()?.addRange(range);
	await page.getByRole('button', { name: 'Bold', exact: true }).click();
	await expect.poll(() => element.querySelector('strong')?.textContent).toBe('Selected text');
	await page.getByRole('button', { name: 'Undo', exact: true }).click();
	await expect.poll(() => element.querySelector('strong')).toBeNull();
});

it('round trips checked legacy items and keeps table editing functional', async () => {
	const note = notesStore.createNote('Structured');
	notesStore.updateNote(note.id, {
		content: '<ul class="checklist"><li><input type="checkbox" checked><span>Done</span></li></ul>'
	});
	renderNotes();
	await expect.element(page.getByRole('checkbox')).toBeChecked();
	await page.getByRole('checkbox').click();
	await expect.poll(() => notesStore.activeNote?.content).toContain('data-checked="false"');
	await page.getByRole('button', { name: 'Insert table', exact: true }).click();
	await page.getByRole('button', { name: 'Insert', exact: true }).click();
	await expect.poll(() => document.querySelectorAll('.note-prose td').length).toBe(9);
	await page.getByRole('button', { name: 'Row below', exact: true }).click();
	await expect.poll(() => document.querySelectorAll('.note-prose td').length).toBe(12);
	await page.getByRole('button', { name: 'Delete table', exact: true }).click();
	await expect.poll(() => document.querySelector('.note-prose table')).toBeNull();
});

it('retains a late attachment after leaving, returning, and typing in the original note', async () => {
	const other = notesStore.createNote('Attachment other');
	const original = notesStore.createNote('Attachment original');
	renderNotes();
	await expect.element(page.getByRole('textbox', { name: 'Note editor', exact: true })).toBeVisible();
	let finish!: (text: string) => void;
	vi.spyOn(File.prototype, 'text').mockImplementation(() => new Promise(resolve => { finish = resolve; }));
	const input = document.querySelector('input[type=file]:not([accept])') as HTMLInputElement;
	const transfer = new DataTransfer(); transfer.items.add(new File(['attachment'], 'late.txt', { type: 'text/plain' }));
	input.files = transfer.files; input.dispatchEvent(new Event('change', { bubbles: true }));
	await page.getByRole('button', { name: 'Open Attachment other', exact: true }).click();
	await page.getByRole('button', { name: 'Open Attachment original', exact: true }).click();
	await page.getByRole('textbox', { name: 'Note editor', exact: true }).fill('New draft');
	finish('Late attachment text');
	await expect.poll(() => notesStore.notes.find(n => n.id === original.id)?.content, { timeout: 1500 }).toContain('Late attachment text');
	await new Promise(resolve => setTimeout(resolve, 650));
	expect(notesStore.notes.find(n => n.id === original.id)?.content).toContain('Late attachment text');
	expect(notesStore.notes.find(n => n.id === original.id)?.content).toContain('New draft');
	expect(notesStore.notes.find(n => n.id === other.id)?.content).toBe('');
});

it('includes text attachments when exporting a note', async () => {
	const note = notesStore.createNote('Export attachment');
	notesStore.updateNote(note.id, { content: '<div class="file-attachment"><div class="file-header">plan.txt</div><pre>Attachment details</pre></div>' });
	renderNotes();
	let blob: Blob | undefined;
	vi.spyOn(URL, 'createObjectURL').mockImplementation(value => { blob = value as Blob; return 'blob:example'; });
	vi.spyOn(HTMLAnchorElement.prototype, 'click').mockImplementation(() => {});
	vi.spyOn(URL, 'revokeObjectURL').mockImplementation(() => {});
	await page.getByRole('button', { name: 'Export', exact: true }).click();
	expect(await blob?.text()).toContain('Attachment details');
});
