import { describe, expect, it } from 'vitest';
import {
	findModelOption,
	isValidModelName,
	modelDisplayName,
	normalizeModelName
} from './model-names';

describe('normalizeModelName', () => {
	it('extracts filename from forward slash path', () => {
		expect(normalizeModelName('models/model-name-1')).toBe('model-name-1');
		expect(normalizeModelName('path/to/model/model-name-2')).toBe('model-name-2');
	});

	it('extracts filename from backslash path', () => {
		expect(normalizeModelName('C\\Models\\model-name-1')).toBe('model-name-1');
		expect(normalizeModelName('path\\to\\model\\model-name-2')).toBe('model-name-2');
	});

	it('handles mixed path separators', () => {
		expect(normalizeModelName('path/to\\model/model-name-2')).toBe('model-name-2');
	});

	it('returns simple names as-is', () => {
		expect(normalizeModelName('simple-model')).toBe('simple-model');
		expect(normalizeModelName('model-name-2')).toBe('model-name-2');
	});

	it('trims whitespace', () => {
		expect(normalizeModelName('  model-name  ')).toBe('model-name');
	});

	it('returns empty string for empty input', () => {
		expect(normalizeModelName('')).toBe('');
		expect(normalizeModelName('   ')).toBe('');
	});
});

describe('isValidModelName', () => {
	it('returns true for valid names', () => {
		expect(isValidModelName('model')).toBe(true);
		expect(isValidModelName('path/to/model.bin')).toBe(true);
	});

	it('returns false for empty values', () => {
		expect(isValidModelName('')).toBe(false);
		expect(isValidModelName('   ')).toBe(false);
	});
});

describe('findModelOption and modelDisplayName', () => {
	const options = [
		{
			id: 'qwen2.5:0.5b',
			name: 'Qwen 2.5 0.5B',
			model: 'qwen2.5:0.5b',
			alias: 'qwen2.5-0.5b-instruct-q4_k_m',
			capabilities: []
		},
		{ id: 'gemma3:270m', name: 'Gemma 3 270M', model: 'gemma3:270m', capabilities: [] }
	];

	it('finds the picker option by registry name, file stem or path', () => {
		expect(findModelOption('qwen2.5:0.5b', options)?.name).toBe('Qwen 2.5 0.5B');
		expect(findModelOption('qwen2.5-0.5b-instruct-q4_k_m', options)?.name).toBe('Qwen 2.5 0.5B');
		expect(findModelOption('/models/qwen2.5-0.5b-instruct-q4_k_m.gguf', options)?.name).toBe(
			'Qwen 2.5 0.5B'
		);
		expect(findModelOption('gemma3-270m', options)?.name).toBe('Gemma 3 270M');
		expect(findModelOption('unknown', options)).toBeUndefined();
	});

	it('recognises a file name when the engine sent no alias', () => {
		const noAlias = [
			{ id: 'gemma3:270m', name: 'Gemma 3 270M', model: 'gemma3:270m', capabilities: [] },
			{ id: 'gemma3qat:270m', name: 'Gemma 3 270M QAT', model: 'gemma3qat:270m', capabilities: [] },
			{ id: 'qwen2.5:0.5b', name: 'Qwen 2.5 0.5B', model: 'qwen2.5:0.5b', capabilities: [] },
			{ id: 'qwen2.5:7b', name: 'Qwen 2.5 7B', model: 'qwen2.5:7b', capabilities: [] }
		];
		expect(modelDisplayName('gemma-3-270m-it-Q8_0', noAlias)).toBe('Gemma 3 270M');
		expect(modelDisplayName('qwen2.5-0.5b-instruct-q4_k_m', noAlias)).toBe('Qwen 2.5 0.5B');
		expect(modelDisplayName('qwen2.5-7b-instruct-q4_k_m.gguf', noAlias)).toBe('Qwen 2.5 7B');
		expect(modelDisplayName('llama-3.2-1b-q4', noAlias)).toBe('llama-3.2-1b-q4');
	});

	it('labels replies with the picker name, or the bare file name when unknown', () => {
		expect(modelDisplayName('qwen2.5-0.5b-instruct-q4_k_m', options)).toBe('Qwen 2.5 0.5B');
		expect(modelDisplayName('/x/other-model.gguf', options)).toBe('other-model');
		expect(modelDisplayName(undefined, options)).toBe('Unknown model');
	});
});
