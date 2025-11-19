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

		return response.json() as Promise<ModelListResponse>;
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

		return response.json() as Promise<ModelListResponse>;
	}

	/**
	 * Download a model
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
