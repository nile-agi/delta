import type { ModelOption } from '$lib/types/models';

/**
 * Normalizes a model name by extracting the filename from a path.
 *
 * Handles both forward slashes (/) and backslashes (\) as path separators.
 * If the model name is just a filename (no path), returns it as-is.
 *
 * @param modelName - The model name or path to normalize
 * @returns The normalized model name (filename only)
 *
 * @example
 * normalizeModelName('models/llama-3.1-8b') // Returns: 'llama-3.1-8b'
 * normalizeModelName('C:\\Models\\gpt-4') // Returns: 'gpt-4'
 * normalizeModelName('simple-model') // Returns: 'simple-model'
 * normalizeModelName('  spaced  ') // Returns: 'spaced'
 * normalizeModelName('') // Returns: ''
 */
export function normalizeModelName(modelName: string): string {
	const trimmed = modelName.trim();

	if (!trimmed) {
		return '';
	}

	const segments = trimmed.split(/[\\/]/);
	const candidate = segments.pop();
	const normalized = candidate?.trim();

	return normalized && normalized.length > 0 ? normalized : trimmed;
}

/**
 * Validates if a model name is valid (non-empty after normalization).
 *
 * @param modelName - The model name to validate
 * @returns true if valid, false otherwise
 */
export function isValidModelName(modelName: string): boolean {
	return normalizeModelName(modelName).length > 0;
}

const canonical = (id: string) =>
	normalizeModelName(id ?? '')
		.toLowerCase()
		.replace(/\.gguf$/, '')
		.replace(/\s+/g, '')
		.replace(/:/g, '-');

/**
 * The picker option a model id refers to, whether it is the registry name ("qwen2.5:0.5b"),
 * the file stem llama-server reports ("qwen2.5-0.5b-instruct-q4_k_m") or a path to the file.
 */
export function findModelOption(id: string | undefined, options: ModelOption[]): ModelOption | undefined {
	if (!id) return undefined;
	const exact = options.find((o) => o.id === id || o.model === id || o.alias === id);
	if (exact) return exact;
	const key = canonical(id);
	const same = options.find(
		(o) => canonical(o.id) === key || canonical(o.model) === key || (!!o.alias && canonical(o.alias) === key)
	);
	if (same) return same;
	// An engine that sends no alias leaves only the file name ("gemma-3-270m-it-Q8_0"); it starts
	// with the registry name once punctuation is dropped ("gemma3:270m"). The longest such name wins.
	const compact = (s: string) => canonical(s).replace(/[^\p{L}\p{N}]/gu, '');
	const file = compact(id);
	let best: ModelOption | undefined;
	let bestLength = 0;
	for (const o of options) {
		const name = compact(o.model || o.id);
		if (name.length >= 4 && file.startsWith(name) && name.length > bestLength) {
			best = o;
			bestLength = name.length;
		}
	}
	return best;
}

/** The name to show for a model: the picker's name when known, else the bare file name. */
export function modelDisplayName(id: string | undefined, options: ModelOption[]): string {
	if (!id) return 'Unknown model';
	return findModelOption(id, options)?.name ?? normalizeModelName(id).replace(/\.gguf$/i, '');
}
