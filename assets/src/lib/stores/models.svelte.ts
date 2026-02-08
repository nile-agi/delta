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
	/** True only when the backend has a model loaded (from select() loaded:true or main API /v1/models). */
	private _modelLoadedOnServer = $state(false);
	private _persistedSelection = persisted<PersistedModelSelection | null>(
		SELECTED_MODEL_LOCALSTORAGE_KEY,
		null
	);

	constructor() {
		// Do not restore selection from localStorage here - let fetch() set it only when the backend has a model loaded.
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

	get modelLoadedOnServer(): boolean {
		return this._modelLoadedOnServer;
	}

	async fetch(force = false): Promise<void> {
		if (this._loading) return;
		if (this._models.length > 0 && !force) return;

		this._loading = true;
		this._error = null;

		try {
			// Fetch installed models from model management API (single source of truth for the list).
			// Optionally fetch main API only to detect currently loaded model for initial selection.
			const [mainResponse, installedModelsResponse] = await Promise.allSettled([
				ModelsService.list(),
				ModelsService.listInstalled().catch(() => null) // Gracefully handle if model API server is not running
			]);

			// Build options only from installed models so each model appears once with display_name.
			const installedModelsMap = new SvelteMap<string, ModelOption>();
			if (
				installedModelsResponse.status === 'fulfilled' &&
				installedModelsResponse.value !== null
			) {
				const responseData = installedModelsResponse.value;
				const installed = Array.isArray(responseData) ? responseData : responseData.models || [];

				if (!Array.isArray(installed)) {
					console.error('Invalid response format from model API:', responseData);
					throw new Error('Invalid response format from model management API');
				}

				for (const modelInfo of installed) {
					const modelId = modelInfo.name;
					const displayName = modelInfo.display_name || this.toDisplayName(modelInfo.name);

					installedModelsMap.set(modelId, {
						id: modelId,
						name: displayName,
						model: modelInfo.name,
						description: modelInfo.description,
						capabilities: [],
						details: {
							quantization_level: modelInfo.quantization
						},
						meta: null
					});
				}
			}

			const models = Array.from(installedModelsMap.values());
			models.sort((a, b) => a.name.localeCompare(b.name));
			this._models = models;

			// Normalize model id for matching (e.g. "qwen3-0.6b" and "qwen3:0.6b" -> same canonical form).
			const normalizeId = (id: string) =>
				(id ?? '').toLowerCase().replace(/\s+/g, '').replace(/:/g, '-');
			const findOptionByMainId = (mainId: string) =>
				models.find(
					(opt) =>
						opt.id === mainId ||
						opt.model === mainId ||
						normalizeId(opt.model) === normalizeId(mainId)
				);

			if (!force) {
				// Initial load: only set selection when the main API reports a model is loaded (llama-server running).
				// Otherwise leave selection empty so the user must choose a model.
				if (mainResponse.status === 'fulfilled' && mainResponse.value?.data?.length > 0) {
					const firstMainId = mainResponse.value.data[0]?.id;
					const matched = firstMainId ? findOptionByMainId(firstMainId) : null;
					if (matched) {
						this._selectedModelId = matched.id;
						this._selectedModelName = matched.model;
						this._persistedSelection.value = { id: matched.id, model: matched.model };
						this._modelLoadedOnServer = true;
					} else {
						this._selectedModelId = null;
						this._selectedModelName = null;
						this._persistedSelection.value = null;
						this._modelLoadedOnServer = false;
					}
				} else {
					this._selectedModelId = null;
					this._selectedModelName = null;
					this._persistedSelection.value = null;
					this._modelLoadedOnServer = false;
				}
			} else {
				const stillPresent = this._models.some((m) => m.id === this._selectedModelId);
				if (!stillPresent) {
					const byModel = this._models.find(
						(m) => m.model === this._selectedModelName || normalizeId(m.model) === normalizeId(this._selectedModelName ?? '')
					);
					if (byModel) {
						this._selectedModelId = byModel.id;
						this._selectedModelName = byModel.model;
						this._persistedSelection.value = { id: byModel.id, model: byModel.model };
					} else {
						this._selectedModelId = null;
						this._selectedModelName = null;
						this._persistedSelection.value = null;
					}
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
				this._modelLoadedOnServer = !!useResponse.loaded;
				if (useResponse.loaded) {
					// Server restarted with new model (same as /use in terminal). Refetch so main API shows current model.
					await this.fetch(true);
					// Update Context stat immediately to loaded model's n_ctx (from llama-server -c).
					if (useResponse.ctx_size != null && useResponse.ctx_size > 0) {
						slotsService.setLoadedContextTotal(useResponse.ctx_size);
					}
					// When model is loaded, llama-server is on 8080, so model API must be on 8081
					// Force model API base URL to 8081 if we're on port 8080
					if (typeof window !== 'undefined' && window.location.port === '8080') {
						const { forceModelApi8081 } = await import('$lib/utils/model-api-url');
						forceModelApi8081();
					}
				} else {
					// Migration in progress (UI-only -> llama-server). Poll so we show green dot once server is up.
					if (typeof window !== 'undefined') {
						const pollInterval = 2000;
						const maxAttempts = 30;
						let attempts = 0;
						const checkLoaded = async () => {
							attempts++;
							try {
								const list = await ModelsService.list();
								if (list?.data?.length > 0) {
									this._modelLoadedOnServer = true;
									await this.fetch(true);
									if (typeof window !== 'undefined' && window.location.port === '8080') {
										const { forceModelApi8081 } = await import('$lib/utils/model-api-url');
										forceModelApi8081();
									}
									return;
								}
							} catch {
								// Ignore
							}
							if (attempts < maxAttempts && this._selectedModelId === option.id) {
								setTimeout(checkLoaded, pollInterval);
							}
						};
						setTimeout(checkLoaded, pollInterval);
					}
				}
			} catch (error) {
				console.warn('Failed to switch model:', error);
				this._error = error instanceof Error ? error.message : String(error);
				// Still set selection so chat can use this model (e.g. router mode loads on demand)
				this._selectedModelId = option.id;
				this._selectedModelName = option.model;
				this._persistedSelection.value = { id: option.id, model: option.model };
				this._modelLoadedOnServer = false;
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
		this._modelLoadedOnServer = false;
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
export const modelLoadedOnServer = () => modelsStore.modelLoadedOnServer;

export const fetchModels = modelsStore.fetch.bind(modelsStore);
export const selectModel = modelsStore.select.bind(modelsStore);
export const unloadModel = modelsStore.unload.bind(modelsStore);
