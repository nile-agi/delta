import { describe, expect, it } from 'vitest';
import { findLoadedModelId, isRouterModelList } from './model-load-state';
import type { ApiModelDataEntry, ApiModelStatus } from '$lib/types/api';

const entry = (id: string, status?: ApiModelStatus): ApiModelDataEntry => ({
	id,
	object: 'model',
	created: 0,
	owned_by: 'llamacpp',
	...(status ? { status } : {})
});

describe('isRouterModelList', () => {
	it('is true when any entry carries a status object', () => {
		expect(isRouterModelList([entry('a'), entry('b', { value: 'unloaded' })])).toBe(true);
	});

	it('is false for a plain OpenAI-style list', () => {
		expect(isRouterModelList([entry('a'), entry('b')])).toBe(false);
	});

	it('is false for an empty list', () => {
		expect(isRouterModelList([])).toBe(false);
	});
});

describe('findLoadedModelId', () => {
	it('returns null when the router lists models but none are loaded', () => {
		const data = [
			entry('qwen3-0.6b', { value: 'unloaded' }),
			entry('gemma3-1b', { value: 'unloaded' })
		];

		expect(findLoadedModelId(data)).toBeNull();
	});

	it('returns the loaded model even when it is not first', () => {
		const data = [
			entry('qwen3-0.6b', { value: 'unloaded' }),
			entry('gemma3-1b', { value: 'loaded' })
		];

		expect(findLoadedModelId(data)).toBe('gemma3-1b');
	});

	it('treats a sleeping model as loaded', () => {
		const data = [entry('qwen3-0.6b', { value: 'sleeping' })];

		expect(findLoadedModelId(data)).toBe('qwen3-0.6b');
	});

	it('returns null while a model is still loading', () => {
		const data = [entry('qwen3-0.6b', { value: 'loading' })];

		expect(findLoadedModelId(data)).toBeNull();
	});

	it('returns null for a model that failed to load', () => {
		const data = [entry('qwen3-0.6b', { value: 'unloaded', failed: true, exit_code: 1 })];

		expect(findLoadedModelId(data)).toBeNull();
	});

	// Single-model mode omits status, and llama.cpp answers every endpoint with 503
	// until the model is ready, so a listed model there is a loaded model.
	it('returns the first entry when the server reports no status at all', () => {
		expect(findLoadedModelId([entry('qwen3-0.6b'), entry('gemma3-1b')])).toBe('qwen3-0.6b');
	});

	it('returns null for an empty list', () => {
		expect(findLoadedModelId([])).toBeNull();
	});
});
