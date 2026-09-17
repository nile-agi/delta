import { afterEach, expect, it, vi } from 'vitest';
import { render, cleanup } from 'vitest-browser-svelte';
import { page } from '@vitest/browser/context';
import Notes from './Notes.svelte';
import { notesStore } from '$lib/stores/notes.svelte';

afterEach(() => {
	cleanup();
	vi.restoreAllMocks();
});

it('inserts emoji into the body after title focus without changing the note icon', async () => {
	const note = notesStore.createNote('Emoji test');
	render(Notes);
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
	render(Notes);
	await expect
		.element(page.getByRole('textbox', { name: 'Note editor', exact: true }))
		.toHaveTextContent('Original plain text');
	await new Promise((resolve) => setTimeout(resolve, 650));
	expect(notesStore.activeNote).toEqual(original);
});

it('saves edits to their original note when switching immediately', async () => {
	const other = notesStore.createNote('Other note');
	const first = notesStore.createNote('First note');
	render(Notes);
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
	render(Notes);
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
	render(Notes);
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
