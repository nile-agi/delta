import { afterEach, expect, it, vi } from 'vitest';
import { cleanup, render } from 'vitest-browser-svelte';
import { page } from '@vitest/browser/context';
import MarkdownContent from './MarkdownContent.svelte';

afterEach(() => cleanup());

it('turns citations into buttons that open the cited place', async () => {
	const onCitation = vi.fn();
	render(MarkdownContent, {
		content: 'Every 400 hours [service.pdf, p.17]. See `[code.pdf, p.2]` too.',
		onCitation
	});
	const chip = page.getByRole('button', { name: 'service.pdf, p.17' });
	await expect.element(chip).toBeVisible();
	await chip.click();
	expect(onCitation).toHaveBeenCalledWith(
		expect.objectContaining({ name: 'service.pdf', pageStart: 17, pageEnd: 17 })
	);
	await expect.element(page.getByRole('button', { name: 'code.pdf, p.2' })).not.toBeInTheDocument();
});

it('leaves citations as text while streaming or without a handler', async () => {
	render(MarkdownContent, { content: 'Every 400 hours [service.pdf, p.17].', streaming: true, onCitation: vi.fn() });
	await expect.element(page.getByText(/service\.pdf, p\.17/)).toBeVisible();
	await expect.element(page.getByRole('button', { name: 'service.pdf, p.17' })).not.toBeInTheDocument();
});
