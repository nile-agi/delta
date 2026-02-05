import { SvelteMap } from 'svelte/reactivity';
import { ModelsService } from '$lib/services/models';
import { slotsService } from '$lib/services/slots';
import { persisted } from '$lib/stores/persisted.svelte';
import { SELECTED_MODEL_LOCALSTORAGE_KEY } from '$lib/constants/localstorage-keys';
import type { ModelOption } from '$lib/types/models';

type PersistedModelSelection = {
	id: string;
	model: string;
};

class ModelsStore {
	private _models = $state<ModelOption[]>([]);
	private _loading = $state(false);
	private _updating = $state(false);
	private _loadingModelId = $state<string | null>(null);
	private _error = $state<string | null>(null);
	private _selectedModelId = $state<string | null>(null);
	private _selectedModelName = $state<string | null>(null);
	private _persistedSelection = persisted<PersistedModelSelection | null>(
		SELECTED_MODEL_LOCALSTORAGE_KEY,
		null
	);

	constructor() {
		const persisted = this._persistedSelection.value;
		if (persisted) {
			this._selectedModelId = persisted.id;
			this._selectedModelName = persisted.model;
		}
	}

	get models(): ModelOption[] {
		return this._models;
	}

	get loading(): boolean {
		return this._loading;
	}

	get updating(): boolean {
		return this._updating;
	}

	get loadingModelId(): string | null {
		return this._loadingModelId;
	}

	get error(): string | null {
		return this._error;
	}

	get selectedModelId(): string | null {
		return this._selectedModelId;
	}

	get selectedModelName(): string | null {
		return this._selectedModelName;
	}

	get selectedModel(): ModelOption | null {
		if (!this._selectedModelId) {
			return null;
		}

		return this._models.find((model) => model.id === this._selectedModelId) ?? null;
	}

	async fetch(force = false): Promise<void> {
		if (this._loading) return;
		if (this._models.length > 0 && !force) return;

		this._loading = true;
		this._error = null;

		try {
			// Fetch from both APIs: main API (currently loaded model) and model management API (all installed models)
			const [mainResponse, installedModelsResponse] = await Promise.allSettled([
				ModelsService.list(),
				ModelsService.listInstalled().catch(() => null) // Gracefully handle if model API server is not running
			]);

			// Get models from main API (currently loaded model)
			const mainModels: ModelOption[] = [];
			if (mainResponse.status === 'fulfilled') {
				const response = mainResponse.value;
				mainModels.push(
					...response.data.map((item, index) => {
						const details = response.models?.[index];
						const rawCapabilities = Array.isArray(details?.capabilities)
							? details?.capabilities
							: [];
						const displayNameSource =
							details?.name && details.name.trim().length > 0 ? details.name : item.id;
						const displayName = this.toDisplayName(displayNameSource);

						return {
							id: item.id,
							name: displayName,
							model: details?.model || item.id,
							description: details?.description,
							capabilities: rawCapabilities.filter((value): value is string => Boolean(value)),
							details: details?.details,
							meta: item.meta ?? null
						} satisfies ModelOption;
					})
				);
			}

			// Get all installed models from model management API
			const installedModelsMap = new SvelteMap<string, ModelOption>();
			if (
				installedModelsResponse.status === 'fulfilled' &&
				installedModelsResponse.value !== null
			) {
				// Handle both formats: {models: [...]} or [...] (for backward compatibility)
				const responseData = installedModelsResponse.value;
				const installed = Array.isArray(responseData) ? responseData : responseData.models || [];

				if (!Array.isArray(installed)) {
					console.error('Invalid response format from model API:', responseData);
					throw new Error('Invalid response format from model management API');
				}

				for (const modelInfo of installed) {
					// Use model name as ID (e.g., "qwen3:0.6b")
					const modelId = modelInfo.name;
					const displayName = modelInfo.display_name || this.toDisplayName(modelInfo.name);

					// Get model path - we'll fetch it when needed, but store the name for now
					// The model name will be used to get the path when selected

					installedModelsMap.set(modelId, {
						id: modelId,
						name: displayName,
						model: modelInfo.name, // Store the model name/ID
						description: modelInfo.description,
						capabilities: [],
						details: {
							quantization_level: modelInfo.quantization
						},
						meta: null
					});
				}
			}

			// Merge: add installed models, but keep main API models if they exist (they have more details)
			for (const mainModel of mainModels) {
				installedModelsMap.set(mainModel.id, mainModel);
			}

			const models = Array.from(installedModelsMap.values());

			// Sort by name for consistent ordering
			models.sort((a, b) => a.name.localeCompare(b.name));

			this._models = models;

			// On initial load (not force-refresh): open in "Select model" mode so user chooses in Web UI.
			// On force-refresh (e.g. after model switch): preserve current selection so it stays loaded.
			if (!force) {
				this._selectedModelId = null;
				this._selectedModelName = null;
				this._persistedSelection.value = null;
			} else {
				// Keep selection valid if the selected model is still in the new list
				const stillPresent = this._models.some((m) => m.id === this._selectedModelId);
				if (!stillPresent) {
					this._selectedModelId = null;
					this._selectedModelName = null;
					this._persistedSelection.value = null;
				}
			}
		} catch (error) {
			this._models = [];
			this._error = error instanceof Error ? error.message : 'Failed to load models';

			throw error;
		} finally {
			this._loading = false;
		}
	}

	async select(modelId: string): Promise<void> {
		if (!modelId || this._updating) {
			return;
		}

		if (this._selectedModelId === modelId) {
			return;
		}

		const option = this._models.find((model) => model.id === modelId);
		if (!option) {
			throw new Error('Selected model is not available');
		}

		this._updating = true;
		this._loadingModelId = modelId;
		this._error = null;

		try {
			// Get the model path from the backend API; pass user's stored context length if set
			const storedCtx =
				typeof window !== 'undefined'
					? (() => {
							const raw = localStorage.getItem('delta_model_ctx_' + option.model);
							if (!raw) return undefined;
							const n = parseInt(raw, 10);
							return Number.isNaN(n) ? undefined : n;
						})()
					: undefined;
			try {
				const useResponse = await ModelsService.use(
					option.model,
					storedCtx != null && storedCtx > 0 ? storedCtx : undefined
				);
				// Prefer model_alias for chat requests (router mode); fallback to model_path or option.model
				const modelForRequests =
					useResponse.model_alias ??
					useResponse.model_name ??
					useResponse.model_path ??
					option.model;

				this._selectedModelId = option.id;
				this._selectedModelName = modelForRequests;
				this._persistedSelection.value = { id: option.id, model: modelForRequests };
				if (useResponse.loaded) {
					// Server restarted with new model (same as /use in terminal). Refetch so main API shows current model.
					await this.fetch(true);
					// Update Context stat immediately to loaded model's n_ctx (from llama-server -c).
					if (useResponse.ctx_size != null && useResponse.ctx_size > 0) {
						slotsService.setLoadedContextTotal(useResponse.ctx_size);
					}
				}
			} catch (error) {
				console.warn('Failed to switch model:', error);
				this._error = error instanceof Error ? error.message : String(error);
				// Still set selection so chat can use this model (e.g. router mode loads on demand)
				this._selectedModelId = option.id;
				this._selectedModelName = option.model;
				this._persistedSelection.value = { id: option.id, model: option.model };
			}
		} finally {
			this._updating = false;
			this._loadingModelId = null;
		}
	}

	private toDisplayName(id: string): string {
		const segments = id.split(/\\|\//);
		const candidate = segments.pop();

		return candidate && candidate.trim().length > 0 ? candidate : id;
	}

	/**
	 * Unloads the current model (clears selection). User must select a model again to send messages.
	 */
	unload(): void {
		this._selectedModelId = null;
		this._selectedModelName = null;
		this._persistedSelection.value = null;
	}
}

export const modelsStore = new ModelsStore();

export const modelOptions = () => modelsStore.models;
export const modelsLoading = () => modelsStore.loading;
export const modelsUpdating = () => modelsStore.updating;
export const loadingModelId = () => modelsStore.loadingModelId;
export const modelsError = () => modelsStore.error;
export const selectedModelId = () => modelsStore.selectedModelId;
export const selectedModelName = () => modelsStore.selectedModelName;
export const selectedModelOption = () => modelsStore.selectedModel;

export const fetchModels = modelsStore.fetch.bind(modelsStore);
export const selectModel = modelsStore.select.bind(modelsStore);
export const unloadModel = modelsStore.unload.bind(modelsStore);
