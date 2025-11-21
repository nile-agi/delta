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
			// Fetch installed models - this is what we want to show in the dropdown
			const installedResponse = await ModelsService.listInstalled();
			const installedModels = Array.isArray(installedResponse)
				? installedResponse
				: installedResponse.models || [];

			// Build display name map
			const displayNameMap = new Map<string, string>();
			for (const model of installedModels) {
				// Map both name (e.g., "qwen3:0.6b") and display_name to display_name
				displayNameMap.set(model.name, model.display_name);
				displayNameMap.set(model.display_name, model.display_name);
			}
			this._displayNameMap = displayNameMap;

			// Also get the currently loaded model from /v1/models to know which one is active
			let currentModelId: string | null = null;
			try {
				const currentResponse = await ModelsService.list();
				if (currentResponse.data && currentResponse.data.length > 0) {
					currentModelId = currentResponse.data[0].id;
				}
			} catch (e) {
				console.warn('Failed to fetch current model from /v1/models:', e);
			}

			// Convert installed models to ModelOption format
			const models: ModelOption[] = installedModels.map((model) => {
				const modelId = model.name; // Use name (e.g., "qwen3:0.6b") as the ID
				const displayName = model.display_name || model.name;

				return {
					id: modelId,
					name: displayName,
					model: model.name, // Use name for switching
					description: model.description || '',
					capabilities: [],
					details: null,
					meta: null
				} satisfies ModelOption;
			});

			this._models = models;

			// Determine selection: prefer current model, then persisted, then first available
			const selection = this.determineInitialSelection(models, currentModelId);

			this._selectedModelId = selection.id;
			this._selectedModelName = selection.model;
			this._persistedSelection.value =
				selection.id && selection.model ? { id: selection.id, model: selection.model } : null;
		} catch (error) {
			this._models = [];
			this._error = error instanceof Error ? error.message : 'Failed to load models';
			console.error('Error fetching models:', error);

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
	 * Priority: current model from server > current selection > persisted selection > first available model > none
	 */
	private determineInitialSelection(models: ModelOption[], currentModelId: string | null = null): {
		id: string | null;
		model: string | null;
	} {
		const persisted = this._persistedSelection.value;
		
		// Priority 1: Current model from server (if it matches an installed model)
		if (currentModelId) {
			// Try to find by exact match
			let match = models.find((m) => m.id === currentModelId || m.model === currentModelId);
			
			// If not found, try to match by name (handle different formats)
			if (!match && currentModelId) {
				// Try matching without .gguf extension or path
				const cleanId = currentModelId.replace(/\.gguf$/, '').split(/[\/\\]/).pop() || currentModelId;
				match = models.find((m) => 
					m.id.includes(cleanId) || 
					m.model.includes(cleanId) ||
					cleanId.includes(m.id) ||
					cleanId.includes(m.model)
				);
			}
			
			if (match) {
				return { id: match.id, model: match.model };
			}
		}
		
		// Priority 2: Current selection
		if (this._selectedModelId) {
			const match = models.find((m) => m.id === this._selectedModelId);
			if (match) {
				return { id: match.id, model: match.model };
			}
		}
		
		// Priority 3: Persisted selection
		if (persisted?.id) {
			const match = models.find((m) => m.id === persisted.id);
			if (match) {
				return { id: match.id, model: match.model };
			}
		}
		
		// Priority 4: First available model
		if (models.length > 0) {
			return { id: models[0].id, model: models[0].model };
		}
		
		// Priority 5: None
		return { id: null, model: null };
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
