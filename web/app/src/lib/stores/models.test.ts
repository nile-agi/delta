import { describe, expect, it, vi, beforeEach } from 'vitest';
import type { ApiModelDataEntry, ApiModelStatus } from '$lib/types/api';

const list = vi.fn();
const listInstalled = vi.fn();

vi.mock('$lib/services/models', async (importOriginal) => {
	const actual = await importOriginal<typeof import('$lib/services/models')>();
	return {
		...actual,
		ModelsService: {
			list: (...a: unknown[]) => list(...a),
			listInstalled: (...a: unknown[]) => listInstalled(...a)
		}
	};
});

const { ModelsStore } = await import('./models.svelte');

const listed = (id: string, status?: ApiModelStatus): ApiModelDataEntry => ({
	id,
	object: 'model',
	created: 0,
	owned_by: 'llamacpp',
	...(status ? { status } : {})
});

const installed = (name: string) => ({
	name,
	display_name: name,
	description: '',
	size_str: '',
	quantization: 'Q8_0',
	size_bytes: 0,
	supports_tools: false
});

describe('ModelsStore.fetch', () => {
	beforeEach(() => {
		list.mockReset();
		listInstalled.mockReset().mockResolvedValue({
			models: [installed('qwen3-0.6b'), installed('gemma3-1b')]
		});
	});

	it('reports no loaded model when the router lists models but none are loaded', async () => {
		list.mockResolvedValue({
			object: 'list',
			data: [
				listed('qwen3-0.6b', { value: 'unloaded' }),
				listed('gemma3-1b', { value: 'unloaded' })
			]
		});
		const store = new ModelsStore();

		await store.fetch();

		expect(store.models).toHaveLength(2);
		expect(store.modelLoadedOnServer).toBe(false);
		expect(store.selectedModelId).toBeNull();
	});

	it('selects the loaded model rather than the first one listed', async () => {
		list.mockResolvedValue({
			object: 'list',
			data: [listed('qwen3-0.6b', { value: 'unloaded' }), listed('gemma3-1b', { value: 'loaded' })]
		});
		const store = new ModelsStore();

		await store.fetch();

		expect(store.modelLoadedOnServer).toBe(true);
		expect(store.selectedModelId).toBe('gemma3-1b');
	});

	it('reports no loaded model when the only listed model failed to load', async () => {
		list.mockResolvedValue({
			object: 'list',
			data: [listed('qwen3-0.6b', { value: 'unloaded', failed: true, exit_code: 1 })]
		});
		const store = new ModelsStore();

		await store.fetch();

		expect(store.modelLoadedOnServer).toBe(false);
		expect(store.selectedModelId).toBeNull();
	});

	// Single-model mode has no per-model status and 503s until the model is ready.
	it('keeps trusting a status-less list as a loaded model', async () => {
		list.mockResolvedValue({ object: 'list', data: [listed('qwen3-0.6b')] });
		const store = new ModelsStore();

		await store.fetch();

		expect(store.modelLoadedOnServer).toBe(true);
		expect(store.selectedModelId).toBe('qwen3-0.6b');
	});

	it('reports no loaded model when the main API is unreachable', async () => {
		list.mockRejectedValue(new Error('Failed to fetch'));
		const store = new ModelsStore();

		await store.fetch();

		expect(store.models).toHaveLength(2);
		expect(store.modelLoadedOnServer).toBe(false);
		expect(store.selectedModelId).toBeNull();
	});
});
