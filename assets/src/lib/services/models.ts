import { base } from '$app/paths';
import { config } from '$lib/stores/settings.svelte';
import type { ApiModelListResponse } from '$lib/types/api';
import { getModelApiBaseUrl } from '$lib/utils/model-api-url';

export interface ModelInfo {
	name: string;
	display_name: string;
	description: string;
	size_str: string;
	quantization: string;
	size_bytes: number;
	installed?: boolean;
}

export interface ModelListResponse {
	models: ModelInfo[];
}

export interface ModelOperationResponse {
	success: boolean;
	message?: string;
	model_path?: string;
	model_name?: string;
	/** Alias/short name used by llama-server in router mode for chat requests. */
	model_alias?: string;
	/** True when the server was restarted with the new model (same as /use in terminal). */
	loaded?: boolean;
}

export class ModelsService {
	/**
	 * List models from main server (llama-server).
	 * Uses /v1/models (OpenAI compat). When server is in router mode, this returns multiple models.
	 * Fallback: if /v1/models returns empty data, try /models (llama.cpp router endpoint) and normalize.
	 */
	static async list(): Promise<ApiModelListResponse> {
		const currentConfig = config();
		const apiKey = currentConfig.apiKey?.toString().trim();
		const headers: HeadersInit = {
			...(apiKey ? { Authorization: `Bearer ${apiKey}` } : {})
		};

		const response = await fetch(`${base}/v1/models`, { headers });
		if (!response.ok) {
			throw new Error(`Failed to fetch model list (status ${response.status})`);
		}
		const data = (await response.json()) as ApiModelListResponse;
		// Router mode: if v1/models returned empty, try /models (llama.cpp router endpoint)
		if (data.data && data.data.length === 0) {
			try {
				const modelsRes = await fetch(`${base}/models`, { headers });
				if (modelsRes.ok) {
					const raw = (await modelsRes.json()) as unknown;
					const items = Array.isArray(raw)
						? raw
						: (raw as { items?: unknown[] })?.items ?? (raw as { models?: unknown[] })?.models ?? [];
					if (items.length > 0) {
						data.data = items.map((m: { id?: string; name?: string; path?: string }, i: number) => ({
							id: m.id ?? m.name ?? m.path ?? `model-${i}`,
							object: 'model',
							created: 0,
							owned_by: ''
						}));
					}
				}
			} catch {
				// Ignore /models fallback errors
			}
		}
		return data;
	}

	/**
	 * List all available models (both installed and available to download)
	 */
	static async listAvailable(): Promise<ModelListResponse> {
		const response = await fetch(`${getModelApiBaseUrl()}/api/models/available`, {
			method: 'GET',
			headers: {
				'Content-Type': 'application/json'
			}
		});

		if (!response.ok) {
			throw new Error(`Failed to fetch available models (status ${response.status})`);
		}

		const data = await response.json();
		// Handle both formats: {models: [...]} or [...] (for backward compatibility)
		if (Array.isArray(data)) {
			return { models: data };
		}
		return data as ModelListResponse;
	}

	/**
	 * List only installed models
	 */
	static async listInstalled(): Promise<ModelListResponse> {
		const response = await fetch(`${getModelApiBaseUrl()}/api/models/list`, {
			method: 'GET',
			headers: {
				'Content-Type': 'application/json'
			}
		});

		if (!response.ok) {
			throw new Error(`Failed to fetch installed models (status ${response.status})`);
		}

		const data = await response.json();
		// Handle both formats: {models: [...]} or [...] (for backward compatibility)
		if (Array.isArray(data)) {
			return { models: data };
		}
		return data as ModelListResponse;
	}

	/**
	 * Get download progress for a model
	 */
	static async getDownloadProgress(modelName: string): Promise<{
		progress: number;
		current_bytes: number;
		total_bytes: number;
		completed: boolean;
		failed: boolean;
		error_message?: string;
	}> {
		try {
			const url = `${getModelApiBaseUrl()}/api/models/download/progress/${encodeURIComponent(modelName)}`;
			console.log('[ModelsService] Fetching progress from:', url);
			const response = await fetch(url, {
				method: 'GET',
				headers: {
					'Content-Type': 'application/json'
				}
			});

			if (!response.ok) {
				console.error('[ModelsService] Progress fetch failed:', response.status, response.statusText);
				throw new Error(`Failed to get download progress (status ${response.status})`);
			}

			const data = await response.json();
			console.log('[ModelsService] Progress data received:', data);
			return data;
		} catch (error) {
			console.error('[ModelsService] Error fetching progress:', error);
			throw error;
		}
	}

	/**
	 * Download a model (returns immediately, use getDownloadProgress to track)
	 */
	static async download(modelName: string): Promise<ModelOperationResponse> {
		const response = await fetch(`${getModelApiBaseUrl()}/api/models/download`, {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json'
			},
			body: JSON.stringify({ model: modelName })
		});

		if (!response.ok) {
			const error = await response.json().catch(() => ({}));
			throw new Error(
				error.error?.message || `Failed to download model (status ${response.status})`
			);
		}

		return response.json() as Promise<ModelOperationResponse>;
	}

	/**
	 * Remove a model
	 */
	static async remove(modelName: string): Promise<ModelOperationResponse> {
		const response = await fetch(`${getModelApiBaseUrl()}/api/models/${encodeURIComponent(modelName)}`, {
			method: 'DELETE',
			headers: {
				'Content-Type': 'application/json'
			}
		});

		if (!response.ok) {
			const error = await response.json().catch(() => ({}));
			throw new Error(
				error.error?.message || `Failed to remove model (status ${response.status})`
			);
		}

		return response.json() as Promise<ModelOperationResponse>;
	}

	/**
	 * Switch to a model (returns model path, but server restart is required)
	 */
	static async use(modelName: string): Promise<ModelOperationResponse> {
		const response = await fetch(`${getModelApiBaseUrl()}/api/models/use`, {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json'
			},
			body: JSON.stringify({ model: modelName })
		});

		if (!response.ok) {
			const error = await response.json().catch(() => ({}));
			throw new Error(
				error.error?.message || `Failed to switch model (status ${response.status})`
			);
		}

		return response.json() as Promise<ModelOperationResponse>;
	}

	/**
	 * Unload model and stop llama-server (releases model in background)
	 */
	static async unload(): Promise<ModelOperationResponse> {
		const response = await fetch(`${getModelApiBaseUrl()}/api/models/unload`, {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json'
			}
		});

		if (!response.ok) {
			const error = await response.json().catch(() => ({}));
			throw new Error(
				error.error?.message || `Failed to unload model (status ${response.status})`
			);
		}

		return response.json() as Promise<ModelOperationResponse>;
	}
}
