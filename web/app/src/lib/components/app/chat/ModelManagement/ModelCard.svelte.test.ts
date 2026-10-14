import { afterEach, expect, it, vi } from 'vitest';
import { cleanup, render } from 'vitest-browser-svelte';
import { page } from '@vitest/browser/context';
import ModelCard from './ModelCard.svelte';

afterEach(() => cleanup());

const bigModel = {
	name: 'big:30b',
	display_name: 'Big 30B',
	download_url: 'org/big',
	file_size_gb: 10,
	context_size: 32768,
	required_ram_gb: 17,
	quantization: 'Q4_K_M'
};

it('says how much memory a model needs without naming an operating system', async () => {
	const { container } = render(ModelCard, { model: bigModel, systemRAMGB: 8, isInstalled: false });
	await expect.element(page.getByText('Needs 17 GB+ of memory')).toBeVisible();
	await expect.element(page.getByText('Incompatible')).toBeVisible();
	expect(container.textContent).not.toContain('Mac');
});

it('does not block a model when the memory could not be read', async () => {
	const onDownload = vi.fn();
	const { container } = render(ModelCard, {
		model: bigModel,
		systemRAMGB: 0,
		isInstalled: false,
		onDownload
	});
	expect(container.textContent).not.toContain('Needs 17 GB+');
	await expect.element(page.getByText('Incompatible')).not.toBeInTheDocument();
	await expect.element(page.getByText('Q4_K_M')).toBeVisible();
});

it('offers the download when the computer has enough memory', async () => {
	render(ModelCard, { model: bigModel, systemRAMGB: 32, isInstalled: false, onDownload: vi.fn() });
	await expect.element(page.getByText('Incompatible')).not.toBeInTheDocument();
	await expect.element(page.getByText('Q4_K_M')).toBeVisible();
});
