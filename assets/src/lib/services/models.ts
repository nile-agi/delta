import { base } from '$app/paths';
import { config } from '$lib/stores/settings.svelte';
import type { ApiModelListResponse } from '$lib/types/api';

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
	/** True when the server was restarted with the new model (same as /use in terminal). */
	loaded?: boolean;
}

export class ModelsService {
	static async list(): Promise<ApiModelListResponse> {
		const currentConfig = config();
		const apiKey = currentConfig.apiKey?.toString().trim();

		const response = await fetch(`${base}/v1/models`, {
			headers: {
				...(apiKey ? { Authorization: `Bearer ${apiKey}` } : {})
			}
		});

		if (!response.ok) {
			throw new Error(`Failed to fetch model list (status ${response.status})`);
		}

		return response.json() as Promise<ApiModelListResponse>;
	}

	/**
	 * List all available models (both installed and available to download)
	 */
	static async listAvailable(): Promise<ModelListResponse> {
		const response = await fetch('http://localhost:8081/api/models/available', {
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
		const response = await fetch('http://localhost:8081/api/models/list', {
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
			const url = `http://localhost:8081/api/models/download/progress/${encodeURIComponent(modelName)}`;
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
		const response = await fetch('http://localhost:8081/api/models/download', {
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
		const response = await fetch(`http://localhost:8081/api/models/${encodeURIComponent(modelName)}`, {
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
		const response = await fetch('http://localhost:8081/api/models/use', {
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
}
