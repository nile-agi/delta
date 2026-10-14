import { getModelApiBaseUrl } from '$lib/utils/model-api-url';

/** The text reader for scanned pages, as the engine reports it. */
export interface OcrStatus {
	model: { id: string; display_name: string; size_bytes: number };
	installed: boolean;
	downloading: boolean;
	progress: number;
	current_bytes: number;
	total_bytes: number;
	error?: string;
}

function apiUrl(path: string): string {
	return `${getModelApiBaseUrl()}${path}`;
}

async function json<T>(res: Response): Promise<T> {
	const data = await res.json().catch(() => ({}));
	if (!res.ok && res.status !== 409)
		throw new Error(data.error || `Text reader request failed (${res.status})`);
	return data as T;
}

export const ocrService = {
	async status(): Promise<OcrStatus> {
		return json(await fetch(apiUrl('/api/ocr')));
	},

	/** Starts the download; already running or installed is fine too. */
	async download(): Promise<OcrStatus> {
		return json(await fetch(apiUrl('/api/ocr/download'), { method: 'POST' }));
	},

	async cancel(): Promise<OcrStatus> {
		return json(await fetch(apiUrl('/api/ocr/download/cancel'), { method: 'POST' }));
	},

	/** The text on one page image (a PNG data URL). */
	async readPage(image: string, signal?: AbortSignal): Promise<string> {
		const res = await fetch(apiUrl('/api/ocr/page'), {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify({ image }),
			signal
		});
		const data = await res.json().catch(() => ({}));
		if (!res.ok) throw new Error(data.error || `The text reader failed (${res.status})`);
		return data.text ?? '';
	}
};
