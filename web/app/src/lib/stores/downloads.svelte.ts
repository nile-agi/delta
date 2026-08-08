import { browser } from '$app/environment';
import { toast } from 'svelte-sonner';
import { ModelApiError, ModelsService } from '$lib/services/models';
import { fetchModels } from '$lib/stores/models.svelte';

export interface DownloadState {
	model: string;
	progress: number;
	currentBytes: number;
	totalBytes: number;
}

const POLL_INTERVAL_MS = 500;
const HYDRATE_ATTEMPTS = 3;
const HYDRATE_RETRY_MS = 2000;

/**
 * Tracks model downloads for the whole app.
 *
 * This is deliberately a module-level singleton. Download state used to live in
 * ModelManagementTab's component-local `$state`, which meant closing the settings window —
 * or merely minimizing it, or switching settings sections — unmounted the component, cleared
 * its poll interval and lost the download, even though the backend kept transferring.
 */
export class DownloadsStore {
	/** Keyed by model name; only ever holds in-flight downloads. */
	private downloads = $state<Record<string, DownloadState>>({});
	private pollTimer: ReturnType<typeof setInterval> | null = null;
	private hydrated = false;
	private polling = false;
	/**
	 * Models the backend has actually reported as in flight. `start()` adds an entry
	 * optimistically, so without this a poll landing while the POST is still in flight would
	 * see the model missing from the backend list and wrongly retire it.
	 */
	private confirmed: Record<string, true> = {};

	/** Bumped whenever a download reaches a terminal state, so views can refresh their lists. */
	completionTick = $state(0);

	get active(): DownloadState[] {
		return Object.values(this.downloads);
	}

	get activeCount(): number {
		return this.active.length;
	}

	get isDownloading(): boolean {
		return this.activeCount > 0;
	}

	isActive(model: string): boolean {
		return model in this.downloads;
	}

	get(model: string): DownloadState | null {
		return this.downloads[model] ?? null;
	}

	/**
	 * Seed from the backend once at startup. This is what survives a full page reload: the
	 * download continues in a detached thread, and GET /api/models/downloads reports it.
	 */
	async hydrate(attemptsLeft = HYDRATE_ATTEMPTS): Promise<void> {
		if (!browser || this.hydrated) return;

		try {
			const active = await ModelsService.listActiveDownloads();
			for (const entry of active) {
				this.confirmed[entry.model] = true;
				this.downloads[entry.model] = toState(entry);
			}
			if (this.activeCount > 0) this.startPolling();
			// Only latch on success. modelApiReady can flip on a 3s timeout with the model API
			// not actually up yet; latching on failure would silently lose reload recovery.
			this.hydrated = true;
		} catch (e) {
			console.error('[downloads] Failed to hydrate active downloads', e);
			if (attemptsLeft > 1) {
				setTimeout(() => void this.hydrate(attemptsLeft - 1), HYDRATE_RETRY_MS);
			}
		}
	}

	async start(model: string): Promise<void> {
		// Show the download immediately rather than waiting a poll cycle.
		this.downloads[model] = { model, progress: 0, currentBytes: 0, totalBytes: 0 };
		this.startPolling();

		try {
			await ModelsService.download(model);
		} catch (e) {
			// 409 means the backend is already downloading this model — adopt it rather than
			// erroring. That is what makes a download started before a reload recoverable.
			if (e instanceof ModelApiError && e.status === 409) return;

			this.drop(model);
			const message = e instanceof Error ? e.message : 'Failed to download model';
			toast.error(message);
			console.error('[downloads] Failed to start download', e);
		}
	}

	async cancel(model: string): Promise<void> {
		try {
			await ModelsService.cancelDownload(model);
		} catch (e) {
			const message = e instanceof Error ? e.message : 'Failed to cancel download';
			toast.error(message);
			console.error('[downloads] Failed to cancel download', e);
		} finally {
			// Drop it locally either way. Removing it here also stops the poll loop from
			// treating the disappearance as a failure and firing an error toast.
			this.drop(model);
		}
	}

	private drop(model: string): void {
		delete this.downloads[model];
		delete this.confirmed[model];
		if (this.activeCount === 0) this.stopPolling();
	}

	private startPolling(): void {
		if (!browser || this.pollTimer !== null) return;
		this.pollTimer = setInterval(() => void this.poll(), POLL_INTERVAL_MS);
	}

	private stopPolling(): void {
		if (this.pollTimer === null) return;
		clearInterval(this.pollTimer);
		this.pollTimer = null;
	}

	/** One request per tick for all downloads, rather than one per tracked model. */
	private async poll(): Promise<void> {
		// setInterval does not wait for the previous tick. Two overlapping polls would each
		// capture the same `previous` snapshot and both resolve the same disappearance,
		// double-firing the completion toast.
		if (this.polling) return;
		this.polling = true;
		try {
			await this.pollOnce();
		} finally {
			this.polling = false;
		}
	}

	private async pollOnce(): Promise<void> {
		let active: Awaited<ReturnType<typeof ModelsService.listActiveDownloads>>;
		try {
			active = await ModelsService.listActiveDownloads();
		} catch (e) {
			console.error('[downloads] Failed to poll downloads', e);
			return;
		}

		const stillActive: Record<string, true> = {};
		const previous = Object.keys(this.downloads);

		for (const entry of active) {
			stillActive[entry.model] = true;
			this.confirmed[entry.model] = true;
			this.downloads[entry.model] = toState(entry);
		}

		// Anything the backend has confirmed and then dropped has finished, one way or another.
		for (const model of previous) {
			if (stillActive[model]) continue;
			if (!this.confirmed[model]) continue; // still starting up; not a disappearance
			this.drop(model);
			void this.resolveTerminal(model);
		}

		if (this.activeCount === 0) this.stopPolling();
	}

	/** The list endpoint only reports in-flight work, so ask why a download disappeared. */
	private async resolveTerminal(model: string): Promise<void> {
		try {
			const progress = await ModelsService.getDownloadProgress(model);

			if (progress.completed) {
				toast.success(`Model ${model} downloaded successfully`);
				await fetchModels(true);
			} else if (progress.cancelled) {
				// User asked for this; a red toast would be noise.
			} else if (progress.failed) {
				toast.error(progress.error_message || `Download failed for ${model}`);
			}
		} catch (e) {
			console.error('[downloads] Failed to resolve download outcome', e);
		} finally {
			this.completionTick++;
		}
	}
}

function toState(entry: { model: string; progress: number; current_bytes: number; total_bytes: number }): DownloadState {
	return {
		model: entry.model,
		progress: entry.progress,
		currentBytes: entry.current_bytes,
		totalBytes: entry.total_bytes
	};
}

export const downloads = new DownloadsStore();
