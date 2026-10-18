import { modelFailureMessage } from '$lib/utils/failure-reason';
import { SvelteMap } from 'svelte/reactivity';
import { ModelsService, type ModelRuntimeSnapshot } from '$lib/services/models';
import { slotsService } from '$lib/services/slots';
import { persisted } from '$lib/stores/persisted.svelte';
import { SELECTED_MODEL_LOCALSTORAGE_KEY } from '$lib/constants/localstorage-keys';
import type { ModelOption } from '$lib/types/models';
import { serverSupportsTools } from '$lib/stores/server.svelte';
import { config } from '$lib/stores/settings.svelte';
import { findLoadedModelId } from '$lib/utils/model-load-state';

type PersistedModelSelection = {
	id: string;
	model: string;
};

export class ModelsStore {
	private _attemptId = $state<string | null>(null);
	private _selectionGeneration = 0;
	private _runtimeTimer: ReturnType<typeof setTimeout> | null = null;
	get attemptId(): string | null { return this._attemptId; }

	private stopRuntimePolling(): void {
		if (this._runtimeTimer) clearTimeout(this._runtimeTimer);
		this._runtimeTimer = null;
	}

	private followRuntime(attemptId: string, generation: number): void {
		this._attemptId = attemptId;
		const poll = async () => {
			if (generation !== this._selectionGeneration || this._attemptId !== attemptId) return;
			try {
				const snapshot = await ModelsService.getRuntime(attemptId);
				if (generation !== this._selectionGeneration || this._attemptId !== attemptId) return;
				if (snapshot.attempt_id !== attemptId) throw new Error('Model load status belongs to a different attempt.');
				this.applyRuntime(snapshot);
				if (snapshot.state !== 'loading' && snapshot.state !== 'ready') return;
			} catch {
				if (generation !== this._selectionGeneration) return;
				this._modelLoadedOnServer = false;
				this._error = 'Connection lost while checking the model. Reconnecting…';
			}
			if (generation === this._selectionGeneration)
				this._runtimeTimer = setTimeout(poll, 1000);
		};
		this._runtimeTimer = setTimeout(poll, 1000);
	}

	private applyRuntime(snapshot: ModelRuntimeSnapshot): void {
		this._modelLoadedOnServer = snapshot.state === 'ready';
		this._updating = snapshot.state === 'loading';
		this._loadingModelId = this._updating ? this._selectedModelId : null;
		this._error = null;
		if (snapshot.state === 'ready' && snapshot.effective_config) {
			slotsService.setLoadedContextTotal(snapshot.effective_config.context);
		} else if (snapshot.state === 'failed' || snapshot.state === 'timed_out') {
			this._loadExitCode = snapshot.failure?.exit_code ?? null;
			this._error = snapshot.failure?.message ?? 'Model could not load. Try again or choose another model.';
		}
	}

	async cancelLoad(): Promise<void> {
		if (!this._attemptId) return;
		const generation = this._selectionGeneration;
		try {
			const result = await ModelsService.cancelRuntime(this._attemptId);
			if (generation !== this._selectionGeneration) return;
			this.stopRuntimePolling();
			this.applyRuntime(result);
		} catch {
			if (generation === this._selectionGeneration) this._error = 'Could not confirm cancellation. Checking the model…';
		}
	}

	async retryLoad(): Promise<void> {
		if (this._selectedModelId) await this.select(this._selectedModelId);
	}

	private _models = $state<ModelOption[]>([]);
	private _loading = $state(false);
	private _updating = $state(false);
	private _loadingModelId = $state<string | null>(null);
	private _error = $state<string | null>(null);
	private _loadExitCode = $state<number | null>(null);
	get loadExitCode(): number | null { return this._loadExitCode; }
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

	get selectedModelSupportsTools(): boolean {
		return this.selectedModel?.capabilities?.includes('tools') ?? false;
	}

	async fetch(force = false): Promise<void> {
		if (this._loading) return;
		if (this._models.length > 0 && !force) return;

		this._loading = true;
		this._error = null;
		this._loadExitCode = null;

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
						alias: modelInfo.alias,
						description: modelInfo.description,
						capabilities: modelInfo.supports_tools ? ['tools'] : [],
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
				try {
					const runtime = await ModelsService.getRuntime();
					const matched = models.find((model) => model.model === runtime.model_name);
					this._modelLoadedOnServer = false;
					if (runtime.attempt_id && matched) {
						this._selectedModelId = matched.id;
						this._selectedModelName = runtime.model_alias ?? matched.model;
						this._attemptId = runtime.attempt_id;
						this._persistedSelection.value = { id: matched.id, model: this._selectedModelName };
						this.applyRuntime(runtime);
						if (runtime.state === 'loading' || runtime.state === 'ready') this.followRuntime(runtime.attempt_id, ++this._selectionGeneration);
					} else {
						this._selectedModelId = null;
						this._selectedModelName = null;
					}
					return;
				} catch { /* Legacy servers have no runtime endpoint; retain their loaded-model detection. */ }
			}

			if (!force) {
				// Initial load: only set selection when the main API reports a model is loaded (llama-server running).
				// Otherwise leave selection empty so the user must choose a model.
				// Router mode lists every model in the models dir, so the per-model status decides.
				const loadedModelId =
					mainResponse.status === 'fulfilled' ? findLoadedModelId(mainResponse.value?.data) : null;
				if (loadedModelId) {
					const matched = findOptionByMainId(loadedModelId);
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
						(m) =>
							m.model === this._selectedModelName ||
							normalizeId(m.model) === normalizeId(this._selectedModelName ?? '')
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
		if (!modelId) return;
		if (this._selectedModelId === modelId && this._modelLoadedOnServer) return;
		const generation = ++this._selectionGeneration;
		this.stopRuntimePolling();
		this._attemptId = null;
		this._modelLoadedOnServer = false;

		const option = this._models.find((model) => model.id === modelId);
		if (!option) {
			throw new Error('Selected model is not available');
		}

		this._updating = true;
		this._loadingModelId = modelId;
		this._error = null;
		this._loadExitCode = null;

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
			if (generation !== this._selectionGeneration) return;
			const modelForRequests =
				useResponse.model_alias ?? useResponse.model_name ?? useResponse.model_path ?? option.model;

			this._selectedModelId = option.id;
			this._selectedModelName = modelForRequests;
			this._persistedSelection.value = { id: option.id, model: modelForRequests };
			this._modelLoadedOnServer = false;

			if (useResponse.attempt_id) {
				this.followRuntime(useResponse.attempt_id, generation);
				return;
			}

			// Older embedded CLI servers do not implement tracked loading yet.
			if (typeof window !== 'undefined') {
				const pollInterval = useResponse.loaded ? 1000 : 2000;
				const maxAttempts = useResponse.loaded ? 120 : 60;
				let attempts = 0;
				let healthPassed = false;
				let loadTriggered = false;
				const expectedModelId = option.id;
				const modelNameForCheck = modelForRequests;

				const onReady = async () => {
					this._modelLoadedOnServer = true;
					this._updating = false;
					this._loadingModelId = null;
					try {
						const { forceModelApiSeparatePort } = await import('$lib/utils/model-api-url');
						forceModelApiSeparatePort();
						await this.fetch(true);
						if (useResponse.ctx_size != null && useResponse.ctx_size > 0) {
							slotsService.setLoadedContextTotal(useResponse.ctx_size);
						}
					} catch {
						/* state already updated — fetch failure is non-critical */
					}
				};

				const checkReady = async () => {
					if (generation !== this._selectionGeneration || this._selectedModelId !== expectedModelId) return;
					attempts++;
					try {
						if (!healthPassed) {
							const healthy = await ModelsService.checkHealth();
							if (generation !== this._selectionGeneration || this._selectedModelId !== expectedModelId) return;
							if (!healthy) {
								if (attempts < maxAttempts && this._selectedModelId === expectedModelId) {
									setTimeout(checkReady, pollInterval);
								} else if (this._selectedModelId === expectedModelId) {
									this._error = 'Model took too long to load. Try selecting it again.';
									this._updating = false;
									this._loadingModelId = null;
								}
								return;
							}
							healthPassed = true;
						}

						const modelStatus = await ModelsService.checkModelReady(modelNameForCheck);
						if (generation !== this._selectionGeneration || this._selectedModelId !== expectedModelId) return;

						if (modelStatus.ready) {
							await onReady();
							return;
						}
						if (modelStatus.failed) {
							this._loadExitCode = modelStatus.exitCode ?? null;
							this._error = modelFailureMessage(option.name, modelStatus.exitCode);
							this._updating = false;
							this._loadingModelId = null;
							return;
						}
						if (modelStatus.needsLoad && !loadTriggered) {
							loadTriggered = true;
							await ModelsService.triggerModelLoad(
								modelStatus.routerModelName ?? modelNameForCheck
							);
						}
					} catch {
						/* ignore */
					}
					if (attempts < maxAttempts && this._selectedModelId === expectedModelId) {
						setTimeout(checkReady, pollInterval);
					} else if (this._selectedModelId === expectedModelId) {
						this._error = 'Model took too long to load. Try selecting it again.';
						this._updating = false;
						this._loadingModelId = null;
					}
				};
				setTimeout(checkReady, pollInterval);
			} else {
				this._updating = false;
				this._loadingModelId = null;
			}
		} catch (error) {
			if (generation !== this._selectionGeneration) return;
			console.warn('Failed to switch model:', error);
			const rawMessage = error instanceof Error ? error.message : String(error);
			this._error =
				rawMessage === 'Failed to fetch' ||
				(error instanceof TypeError && rawMessage.includes('fetch'))
					? "Cannot reach the server while loading the model. Make sure Delta is running (run 'delta' in terminal). If you just selected a model, wait a few seconds and try again."
					: rawMessage;
			this._selectedModelId = option.id;
			this._selectedModelName = option.model;
			this._persistedSelection.value = { id: option.id, model: option.model };
			this._modelLoadedOnServer = false;
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
		++this._selectionGeneration;
		this.stopRuntimePolling();
		this._attemptId = null;
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
export const selectedModelSupportsTools = () =>
	modelsStore.selectedModelSupportsTools || serverSupportsTools();

// Single source of truth for the toggle: the stored flag is user intent, capability is applied here.
// Both the wrench UI and the request routing must read this, or they can disagree.
export const agentToolsActive = () =>
	config().useAgentTools === true && selectedModelSupportsTools();

export const fetchModels = modelsStore.fetch.bind(modelsStore);
export const selectModel = modelsStore.select.bind(modelsStore);
export const unloadModel = modelsStore.unload.bind(modelsStore);

let _modelDropdownTrigger = $state(0);
export const modelDropdownTrigger = () => _modelDropdownTrigger;
export function requestModelSelection() {
	_modelDropdownTrigger++;
}
