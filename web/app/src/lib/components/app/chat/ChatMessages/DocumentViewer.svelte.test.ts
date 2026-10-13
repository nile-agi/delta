import '../../../../../app.css';
import { afterEach, expect, it } from 'vitest';
import { cleanup, render } from 'vitest-browser-svelte';
import { page } from '@vitest/browser/context';
import DocumentViewer from './DocumentViewer.svelte';
import { tinyPdfBase64 } from '../../../../../tests/tiny-pdf';

afterEach(() => cleanup());

it('opens a cited PDF at the page and highlights the passage', async () => {
	render(DocumentViewer, {
		open: true,
		place: {
			name: 'service.pdf',
			pageStart: 2,
			pageEnd: 2,
			passage: 'The filter is replaced every 400 hours'
		},
		document: {
			name: 'service.pdf',
			mimeType: 'application/pdf',
			base64Data: tinyPdfBase64([
				'Intro and safety notes',
				'The filter is replaced every 400 hours'
			])
		}
	});

	await expect.element(page.getByText('Cited at p.2')).toBeVisible();
	await expect.element(page.getByText('Page 2 of 2')).toBeVisible();
	await expect
		.poll(() => document.querySelectorAll('[data-page="2"] [data-highlight]').length)
		.toBeGreaterThan(0);
	expect(document.querySelectorAll('[data-page="1"] [data-highlight]').length).toBe(0);

	await page.getByRole('button', { name: 'Previous page' }).click();
	await expect.element(page.getByText('Page 1 of 2')).toBeVisible();
	await page.getByRole('button', { name: 'Show cited passage' }).click();
	await expect.element(page.getByText('Page 2 of 2')).toBeVisible();
});

it('opens a text document at the cited section with the passage marked', async () => {
	render(DocumentViewer, {
		open: true,
		place: {
			name: 'notes.md',
			pageStart: 2,
			pageEnd: 2,
			passage: 'compressor filter every 400 hours'
		},
		document: {
			name: 'notes.md',
			content:
				'[Page 1]\nGeneral notes about the site.\n\n[Page 2]\nReplace the compressor filter every 400 hours of use.'
		}
	});

	await expect.element(page.getByText('Cited at p.2')).toBeVisible();
	await expect
		.poll(() => document.querySelector('section[aria-current="location"] mark')?.textContent)
		.toBe('compressor filter every 400 hours');
	expect(document.querySelector('section[aria-current="location"] h3')?.textContent).toBe('Page 2');
});

it('says when the file is not stored on this device', async () => {
	render(DocumentViewer, {
		open: true,
		place: { name: 'gone.pdf', pageStart: 1 },
		document: { name: 'gone.pdf' }
	});
	await expect.element(page.getByRole('alert')).toHaveTextContent('not stored on this device');
});
