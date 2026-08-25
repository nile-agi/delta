import type { ApiModelDataEntry } from '$lib/types/api';

/**
 * Interpretation of the per-model `status` that llama-server reports from `/v1/models`.
 *
 * Only router mode (`--models-dir`) reports a status, and there the list contains every
 * model in the directory whether or not it is loaded. In single-model mode there is no
 * status, but the server answers every endpoint with 503 until the model is ready, so a
 * listed model is a loaded model.
 */
export type ModelLoadState = 'loaded' | 'loading' | 'unloaded' | 'failed' | 'unknown';

const LOADED_VALUES = new Set(['loaded', 'sleeping']);

export function isRouterModelList(data: ApiModelDataEntry[] | undefined | null): boolean {
	return (data ?? []).some((model) => model.status != null && typeof model.status === 'object');
}

export function getModelLoadState(entry: ApiModelDataEntry | undefined | null): ModelLoadState {
	const status = entry?.status;
	if (status == null || typeof status !== 'object') return 'unknown';

	const value = status.value;

	if (value != null && LOADED_VALUES.has(value)) return 'loaded';
	if (status.failed === true) return 'failed';
	if (value === 'unloaded') {
		return status.exit_code != null && status.exit_code !== 0 ? 'failed' : 'unloaded';
	}
	if (value === 'loading') return 'loading';

	return 'unknown';
}

/**
 * Id of the model the server currently has loaded, or null when nothing is loaded.
 */
export function findLoadedModelId(data: ApiModelDataEntry[] | undefined | null): string | null {
	const entries = data ?? [];
	if (entries.length === 0) return null;

	if (!isRouterModelList(entries)) {
		return entries[0]?.id ?? null;
	}

	return entries.find((entry) => getModelLoadState(entry) === 'loaded')?.id ?? null;
}
