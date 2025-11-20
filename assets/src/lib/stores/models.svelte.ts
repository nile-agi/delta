import { ModelsService, type ModelInfo } from '$lib/services/models';
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
	private _error = $state<string | null>(null);
	private _selectedModelId = $state<string | null>(null);
	private _selectedModelName = $state<string | null>(null);
	private _displayNameMap = $state<Map<string, string>>(new Map());
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
			// Fetch installed models to get display_name mapping
			let displayNameMap = new Map<string, string>();
			try {
				const installedResponse = await ModelsService.listInstalled();
				const installedModels = Array.isArray(installedResponse)
					? installedResponse
					: installedResponse.models || [];
				for (const model of installedModels) {
					// Map both name (e.g., "qwen3:0.6b") and display_name to display_name
					displayNameMap.set(model.name, model.display_name);
					displayNameMap.set(model.display_name, model.display_name);
				}
			} catch (e) {
				console.warn('Failed to fetch installed models for display name mapping:', e);
			}
			this._displayNameMap = displayNameMap;

			const response = await ModelsService.list();

			const models: ModelOption[] = response.data.map((item, index) => {
				const details = response.models?.[index];
				const rawCapabilities = Array.isArray(details?.capabilities) ? details?.capabilities : [];
				const modelId = item.id;
				
				// Get display name: prefer from installed models map, then from details.name, then fallback to toDisplayName
				let displayName = this._displayNameMap.get(modelId);
				if (!displayName && details?.name) {
					displayName = this._displayNameMap.get(details.name);
				}
				if (!displayName) {
					// Only use toDisplayName as last resort, and only if it doesn't look like a filename
					const displayNameSource = details?.name && details.name.trim().length > 0 ? details.name : modelId;
					if (!displayNameSource.includes('.gguf') && !displayNameSource.includes('/') && !displayNameSource.includes('\\')) {
						displayName = displayNameSource;
					} else {
						displayName = this.toDisplayName(displayNameSource);
					}
				}

				return {
					id: modelId,
					name: displayName,
					model: details?.model || modelId,
					description: details?.description,
					capabilities: rawCapabilities.filter((value): value is string => Boolean(value)),
					details: details?.details,
					meta: item.meta ?? null
				} satisfies ModelOption;
			});

			this._models = models;

			const selection = this.determineInitialSelection(models);

			this._selectedModelId = selection.id;
			this._selectedModelName = selection.model;
			this._persistedSelection.value =
				selection.id && selection.model ? { id: selection.id, model: selection.model } : null;
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
		this._error = null;

		try {
			// Get the model name (e.g., "qwen3:0.6b") from the option
			// The option.model should be the model name, or we can use option.id
			const modelName = option.model || option.id;
			
			// Call the API to switch models
			try {
				const result = await ModelsService.use(modelName);
				
				// If server is restarting, wait a bit and retry fetching models
				if (result.server_restarted) {
					// Wait for server to restart (give it 2 seconds)
					await new Promise(resolve => setTimeout(resolve, 2000));
					
					// Retry fetching models with exponential backoff
					let retries = 3;
					let delay = 1000;
					while (retries > 0) {
						try {
							await this.fetch(true);
							break;
						} catch (error) {
							retries--;
							if (retries > 0) {
								await new Promise(resolve => setTimeout(resolve, delay));
								delay *= 2; // Exponential backoff
							} else {
								console.warn('Failed to refresh models after server restart, but model switch was successful');
							}
						}
					}
				} else {
					// Server didn't restart, just refresh normally
					await this.fetch(true);
				}
			} catch (error) {
				console.error('Failed to switch model via API:', error);
				// Still update local state even if API call fails
				// The server might restart, so we'll handle connection errors gracefully
			}

			this._selectedModelId = option.id;
			this._selectedModelName = option.model;
			this._persistedSelection.value = { id: option.id, model: option.model };
		} finally {
			this._updating = false;
		}
	}

	private toDisplayName(id: string): string {
		// If it's already a display name (from our map), use it
		if (this._displayNameMap.has(id)) {
			return this._displayNameMap.get(id)!;
		}
		
		// If it looks like a filename (contains .gguf), try to extract a better name
		if (id.includes('.gguf')) {
			// Remove .gguf extension
			let name = id.replace(/\.gguf$/, '');
			// Try to convert common patterns:
			// "Qwen3-1.7B-f16" -> "Qwen 3 1.7B"
			// "qwen2.5-0.5b" -> "Qwen 2.5 0.5B"
			name = name
				.replace(/([a-z])(\d)/g, '$1 $2') // Add space before numbers
				.replace(/(\d)([A-Z])/g, '$1 $2') // Add space between number and capital
				.replace(/-([a-z])/g, (_, letter) => ` ${letter.toUpperCase()}`) // Convert dash to space and capitalize
				.replace(/\b([a-z])/g, (_, letter) => letter.toUpperCase()); // Capitalize first letter of words
			
			return name;
		}
		
		// Otherwise, just extract filename from path
		const segments = id.split(/\\|\//);
		const candidate = segments.pop();

		return candidate && candidate.trim().length > 0 ? candidate : id;
	}

	/**
	 * Determines which model should be selected after fetching the models list.
	 * Priority: current selection > persisted selection > first available model > none
	 */
	private determineInitialSelection(models: ModelOption[]): {
		id: string | null;
		model: string | null;
	} {
		const persisted = this._persistedSelection.value;
		let nextSelectionId = this._selectedModelId ?? persisted?.id ?? null;
		let nextSelectionName = this._selectedModelName ?? persisted?.model ?? null;

		if (nextSelectionId) {
			const match = models.find((m) => m.id === nextSelectionId);

			if (match) {
				nextSelectionId = match.id;
				nextSelectionName = match.model;
			} else if (models[0]) {
				nextSelectionId = models[0].id;
				nextSelectionName = models[0].model;
			} else {
				nextSelectionId = null;
				nextSelectionName = null;
			}
		} else if (models[0]) {
			nextSelectionId = models[0].id;
			nextSelectionName = models[0].model;
		}

		return { id: nextSelectionId, model: nextSelectionName };
	}
}

export const modelsStore = new ModelsStore();

export const modelOptions = () => modelsStore.models;
export const modelsLoading = () => modelsStore.loading;
export const modelsUpdating = () => modelsStore.updating;
export const modelsError = () => modelsStore.error;
export const selectedModelId = () => modelsStore.selectedModelId;
export const selectedModelName = () => modelsStore.selectedModelName;
export const selectedModelOption = () => modelsStore.selectedModel;

export const fetchModels = modelsStore.fetch.bind(modelsStore);
export const selectModel = modelsStore.select.bind(modelsStore);
