import { config } from '$lib/stores/settings.svelte';
import type { ApiModelListResponse } from '$lib/types/api';
import { getModelApiBaseUrl } from '$lib/utils/model-api-url';
import { getServerBaseUrl } from '$lib/utils/server-base-url';

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
	/** Context size (n_ctx) the model was loaded with; from backend -c / llama-server. */
	ctx_size?: number;
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

		const response = await fetch(`${getServerBaseUrl()}/v1/models`, { headers });
		if (!response.ok) {
			throw new Error(`Failed to fetch model list (status ${response.status})`);
		}
		const data = (await response.json()) as ApiModelListResponse;
		// Router mode: if v1/models returned empty, try /models (llama.cpp router endpoint)
		if (data.data && data.data.length === 0) {
			try {
				const modelsRes = await fetch(`${getServerBaseUrl()}/models`, { headers });
				if (modelsRes.ok) {
					const raw = (await modelsRes.json()) as unknown;
					const items = Array.isArray(raw)
						? raw
						: ((raw as { items?: unknown[] })?.items ??
							(raw as { models?: unknown[] })?.models ??
							[]);
					if (items.length > 0) {
						data.data = items.map(
							(m: { id?: string; name?: string; path?: string }, i: number) => ({
								id: m.id ?? m.name ?? m.path ?? `model-${i}`,
								object: 'model',
								created: 0,
								owned_by: ''
							})
						);
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
				console.error(
					'[ModelsService] Progress fetch failed:',
					response.status,
					response.statusText
				);
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
		let response: Response;
		try {
			response = await fetch(`${getModelApiBaseUrl()}/api/models/download`, {
				method: 'POST',
				headers: {
					'Content-Type': 'application/json'
				},
				body: JSON.stringify({ model: modelName })
			});
		} catch (e) {
			const msg =
				e instanceof Error && e.message === 'Failed to fetch'
					? "Cannot reach model server. Make sure Delta is running (e.g. run 'delta' in terminal or start delta-server)."
					: e instanceof Error
						? e.message
						: 'Failed to fetch';
			throw new Error(msg);
		}

		if (!response.ok) {
			const error = await response.json().catch(() => ({}));
			throw new Error(
				error.error?.message || `Failed to download model (status ${response.status})`
			);
		}

		return response.json() as Promise<ModelOperationResponse>;
	}

	/**
	 * Request cancellation of an in-progress download.
	 * This is best-effort: the backend stops the underlying transfer via libcurl.
	 */
	static async cancelDownload(modelName: string): Promise<ModelOperationResponse> {
		const response = await fetch(`${getModelApiBaseUrl()}/api/models/download/cancel`, {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json'
			},
			body: JSON.stringify({ model: modelName })
		});

		if (!response.ok) {
			const error = await response.json().catch(() => ({}));
			throw new Error(
				error.error?.message || `Failed to cancel download (status ${response.status})`
			);
		}

		return response.json() as Promise<ModelOperationResponse>;
	}

	/**
	 * Remove a model
	 */
	static async remove(modelName: string): Promise<ModelOperationResponse> {
		const response = await fetch(
			`${getModelApiBaseUrl()}/api/models/${encodeURIComponent(modelName)}`,
			{
				method: 'DELETE',
				headers: {
					'Content-Type': 'application/json'
				}
			}
		);

		if (!response.ok) {
			const error = await response.json().catch(() => ({}));
			throw new Error(error.error?.message || `Failed to remove model (status ${response.status})`);
		}

		return response.json() as Promise<ModelOperationResponse>;
	}

	/**
	 * Switch to a model (returns model path, but server restart is required).
	 * Optional ctxSize: user's context length choice; persisted on backend and used for -c when loading.
	 */
	static async use(modelName: string, ctxSize?: number): Promise<ModelOperationResponse> {
		const body: { model: string; ctx_size?: number } = { model: modelName };
		if (ctxSize != null && ctxSize > 0) {
			body.ctx_size = ctxSize;
		}
		const url = `${getModelApiBaseUrl()}/api/models/use`;
		const doRequest = async (): Promise<Response> => {
			return fetch(url, {
				method: 'POST',
				headers: {
					'Content-Type': 'application/json'
				},
				body: JSON.stringify(body)
			});
		};

		let response: Response;
		try {
			response = await doRequest();
		} catch (e) {
			const isNetworkError =
				e instanceof TypeError && (e.message === 'Failed to fetch' || e.message.includes('fetch'));
			if (isNetworkError) {
				// Retry once after a short delay (server may be restarting during migration)
				await new Promise((r) => setTimeout(r, 2500));
				try {
					response = await doRequest();
				} catch (retryErr) {
					throw new Error(
						"Cannot reach the server while loading the model. Make sure Delta is running (run 'delta' in terminal). If you just selected a model, the server may be restarting — wait a few seconds and try again."
					);
				}
			} else {
				throw new Error(
					e instanceof Error ? e.message : 'Connection error while loading the model. Please try again.'
				);
			}
		}

		if (!response.ok) {
			const error = await response.json().catch(() => ({}));
			throw new Error(error.error?.message || `Failed to switch model (status ${response.status})`);
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
			throw new Error(error.error?.message || `Failed to unload model (status ${response.status})`);
		}

		return response.json() as Promise<ModelOperationResponse>;
	}

	static async checkHealth(): Promise<boolean> {
		try {
			const res = await fetch(`${getServerBaseUrl()}/health`);
			return res.ok;
		} catch {
			return false;
		}
	}

	static async checkModelReady(
		modelName: string
	): Promise<{ ready: boolean; failed: boolean; needsLoad: boolean; routerModelName?: string }> {
		try {
			const res = await fetch(`${getServerBaseUrl()}/v1/models`);
			if (!res.ok) return { ready: false, failed: false, needsLoad: false };
			const data = await res.json();
			if (!data?.data?.length) return { ready: false, failed: false, needsLoad: false };

			const hasStatusFields = data.data.some(
				(m: Record<string, unknown>) => m.status && typeof m.status === 'object'
			);
			if (!hasStatusFields) {
				return { ready: true, failed: false, needsLoad: false };
			}

			const normalized = modelName.toLowerCase();
			const getId = (m: Record<string, unknown>) => ((m.id as string) || '').toLowerCase();

			let model = data.data.find((m: Record<string, unknown>) => getId(m) === normalized);
			if (!model) {
				const stem = normalized.split(/[/\\]/).pop()?.replace(/\.gguf$/i, '') ?? normalized;
				model = data.data.find((m: Record<string, unknown>) => getId(m) === stem);
			}

			if (!model?.status || typeof model.status !== 'object') {
				return { ready: false, failed: false, needsLoad: false };
			}

			const routerModelName = (model.id as string) || modelName;
			const status = model.status as Record<string, unknown>;
			const value = status.value as string;

			if (value === 'loaded' || value === 'sleeping') {
				return { ready: true, failed: false, needsLoad: false, routerModelName };
			}
			if (status.failed || (value === 'unloaded' && status.exit_code != null && status.exit_code !== 0)) {
				return { ready: false, failed: true, needsLoad: false, routerModelName };
			}
			if (value === 'unloaded') {
				return { ready: false, failed: false, needsLoad: true, routerModelName };
			}
			return { ready: false, failed: false, needsLoad: false, routerModelName };
		} catch {
			return { ready: false, failed: false, needsLoad: false };
		}
	}

	static async triggerModelLoad(modelName: string): Promise<boolean> {
		try {
			const res = await fetch(`${getServerBaseUrl()}/models/load`, {
				method: 'POST',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify({ model: modelName })
			});
			return res.ok;
		} catch {
			return false;
		}
	}

	/**
	 * Get system RAM information
	 */
	static async getSystemRAM(): Promise<{ total_ram_gb: number; total_ram_bytes: number }> {
		const response = await fetch(`${getModelApiBaseUrl()}/api/system/ram`, {
			method: 'GET',
			headers: {
				'Content-Type': 'application/json'
			}
		});

		if (!response.ok) {
			// Fallback: try browser API if available
			if (typeof navigator !== 'undefined' && 'deviceMemory' in navigator) {
				const deviceMemory = (navigator as Navigator & { deviceMemory?: number }).deviceMemory;
				if (deviceMemory) {
					return {
						total_ram_gb: deviceMemory,
						total_ram_bytes: deviceMemory * 1024 * 1024 * 1024
					};
				}
			}
			throw new Error(`Failed to get system RAM (status ${response.status})`);
		}

		return response.json();
	}
}
