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

	static async listAvailable(): Promise<{ models: ModelInfo[] } | ModelInfo[]> {
		const response = await fetch('http://localhost:8081/api/models/available');

		if (!response.ok) {
			throw new Error(`Failed to fetch available models (status ${response.status})`);
		}

		const data = await response.json();
		// Return in consistent format
		return Array.isArray(data) ? { models: data } : data;
	}

	static async listInstalled(): Promise<{ models: ModelInfo[] } | ModelInfo[]> {
		const response = await fetch('http://localhost:8081/api/models/list');

		if (!response.ok) {
			throw new Error(`Failed to fetch installed models (status ${response.status})`);
		}

		const data = await response.json();
		// Return in consistent format
		return Array.isArray(data) ? { models: data } : data;
	}

	static async download(
		modelName: string,
		onProgress?: (progress: number, currentMB: number, totalMB: number) => void
	): Promise<{ success: boolean; message: string }> {
		const response = await fetch('http://localhost:8081/api/models/download', {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json'
			},
			body: JSON.stringify({ model: modelName })
		});

		if (!response.ok) {
			const error = await response.json();
			throw new Error(error.error?.message || `Failed to download model (status ${response.status})`);
		}

		return response.json();
	}

	static async remove(modelName: string): Promise<{ success: boolean; message: string }> {
		const response = await fetch(`http://localhost:8081/api/models/${encodeURIComponent(modelName)}`, {
			method: 'DELETE'
		});

		if (!response.ok) {
			const error = await response.json();
			throw new Error(error.error?.message || `Failed to remove model (status ${response.status})`);
		}

		return response.json();
	}

	static async use(modelName: string): Promise<{ 
		success: boolean; 
		message: string; 
		model_path?: string;
		model_alias?: string;
		server_restarted?: boolean;
	}> {
		const response = await fetch('http://localhost:8081/api/models/use', {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json'
			},
			body: JSON.stringify({ model: modelName })
		});

		if (!response.ok) {
			const error = await response.json();
			throw new Error(error.error?.message || `Failed to switch model (status ${response.status})`);
		}

		return response.json();
	}
}

