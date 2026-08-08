import { describe, expect, it, vi, beforeEach, afterEach } from 'vitest';
import { ModelApiError } from '$lib/services/models';

const listActiveDownloads = vi.fn();
const download = vi.fn();
const cancelDownload = vi.fn();
const getDownloadProgress = vi.fn();
const toastSuccess = vi.fn();
const toastError = vi.fn();

vi.mock('$lib/services/models', async (importOriginal) => {
	const actual = await importOriginal<typeof import('$lib/services/models')>();
	return {
		...actual,
		ModelsService: {
			listActiveDownloads: (...a: unknown[]) => listActiveDownloads(...a),
			download: (...a: unknown[]) => download(...a),
			cancelDownload: (...a: unknown[]) => cancelDownload(...a),
			getDownloadProgress: (...a: unknown[]) => getDownloadProgress(...a)
		}
	};
});

// The store no-ops its polling when not in a browser; this suite exercises that polling.
vi.mock('$app/environment', () => ({ browser: true }));

vi.mock('svelte-sonner', () => ({
	toast: {
		success: (...a: unknown[]) => toastSuccess(...a),
		error: (...a: unknown[]) => toastError(...a)
	}
}));

vi.mock('$lib/stores/models.svelte', () => ({
	fetchModels: vi.fn().mockResolvedValue(undefined)
}));

const { DownloadsStore } = await import('./downloads.svelte');

const entry = (model: string, progress = 0) => ({
	model,
	progress,
	current_bytes: 0,
	total_bytes: 0
});

/** Let queued promise callbacks inside the poll loop settle. */
const flush = async () => {
	for (let i = 0; i < 6; i++) await Promise.resolve();
};

describe('DownloadsStore', () => {
	beforeEach(() => {
		vi.useFakeTimers();
		listActiveDownloads.mockReset().mockResolvedValue([]);
		download.mockReset().mockResolvedValue({ success: true });
		cancelDownload.mockReset().mockResolvedValue({ success: true });
		getDownloadProgress.mockReset();
		toastSuccess.mockReset();
		toastError.mockReset();
	});

	afterEach(() => {
		vi.useRealTimers();
	});

	it('tracks a download immediately, before the first poll returns', async () => {
		const store = new DownloadsStore();
		await store.start('qwen3:0.6b');

		expect(store.activeCount).toBe(1);
		expect(store.isActive('qwen3:0.6b')).toBe(true);
		expect(store.get('qwen3:0.6b')?.progress).toBe(0);
		expect(toastError).not.toHaveBeenCalled();
	});

	it('adopts an already-running download instead of erroring on 409', async () => {
		download.mockRejectedValue(new ModelApiError('Download already in progress', 409));
		const store = new DownloadsStore();

		await store.start('qwen3:0.6b');

		expect(store.isActive('qwen3:0.6b')).toBe(true);
		expect(toastError).not.toHaveBeenCalled();
	});

	it('drops the download and surfaces the error for a genuine failure', async () => {
		download.mockRejectedValue(new ModelApiError('Server exploded', 500));
		const store = new DownloadsStore();

		await store.start('qwen3:0.6b');

		expect(store.isActive('qwen3:0.6b')).toBe(false);
		expect(toastError).toHaveBeenCalledWith('Server exploded');
	});

	it('reports success and bumps completionTick when a download finishes', async () => {
		const store = new DownloadsStore();
		listActiveDownloads.mockResolvedValue([entry('qwen3:0.6b', 10)]);
		await store.start('qwen3:0.6b');
		await vi.advanceTimersByTimeAsync(500);
		await flush();
		const tickBefore = store.completionTick;

		// Backend no longer lists it, and per-model progress says it completed.
		listActiveDownloads.mockResolvedValue([]);
		getDownloadProgress.mockResolvedValue({
			progress: 100,
			current_bytes: 1,
			total_bytes: 1,
			completed: true,
			failed: false
		});

		await vi.advanceTimersByTimeAsync(500);
		await flush();

		expect(store.activeCount).toBe(0);
		expect(toastSuccess).toHaveBeenCalledWith('Model qwen3:0.6b downloaded successfully');
		expect(toastError).not.toHaveBeenCalled();
		expect(store.completionTick).toBeGreaterThan(tickBefore);
	});

	it('surfaces an error toast when a download fails', async () => {
		const store = new DownloadsStore();
		listActiveDownloads.mockResolvedValue([entry('qwen3:0.6b', 10)]);
		await store.start('qwen3:0.6b');
		await vi.advanceTimersByTimeAsync(500);
		await flush();

		listActiveDownloads.mockResolvedValue([]);
		getDownloadProgress.mockResolvedValue({
			progress: 12,
			current_bytes: 1,
			total_bytes: 8,
			completed: false,
			failed: true,
			error_message: 'Download failed'
		});

		await vi.advanceTimersByTimeAsync(500);
		await flush();

		expect(toastError).toHaveBeenCalledWith('Download failed');
	});

	it('does not toast an error when the user cancels', async () => {
		const store = new DownloadsStore();
		await store.start('qwen3:0.6b');

		await store.cancel('qwen3:0.6b');
		expect(store.activeCount).toBe(0);

		// A later poll must not resurrect it or report a failure.
		listActiveDownloads.mockResolvedValue([]);
		await vi.advanceTimersByTimeAsync(500);
		await flush();

		expect(cancelDownload).toHaveBeenCalledWith('qwen3:0.6b');
		expect(toastError).not.toHaveBeenCalled();
		expect(toastSuccess).not.toHaveBeenCalled();
	});

	it('keeps a just-started download that the backend has not listed yet', async () => {
		const store = new DownloadsStore();
		await store.start('qwen3:0.6b');

		// The POST is registered but a poll races ahead of the backend listing it.
		listActiveDownloads.mockResolvedValue([]);
		await vi.advanceTimersByTimeAsync(500);
		await flush();

		expect(store.isActive('qwen3:0.6b')).toBe(true);
		expect(getDownloadProgress).not.toHaveBeenCalled();
		expect(toastSuccess).not.toHaveBeenCalled();
		expect(toastError).not.toHaveBeenCalled();
	});

	it('retries hydration when the model API is not up yet', async () => {
		listActiveDownloads
			.mockRejectedValueOnce(new Error('Failed to fetch'))
			.mockResolvedValue([entry('qwen3:0.6b', 12)]);
		const store = new DownloadsStore();

		await store.hydrate();
		expect(store.activeCount).toBe(0); // first attempt failed

		await vi.advanceTimersByTimeAsync(2000);
		await flush();

		expect(store.activeCount).toBe(1);
		expect(store.get('qwen3:0.6b')?.progress).toBe(12);
	});

	it('does not double-report a completion when polls overlap', async () => {
		const store = new DownloadsStore();
		listActiveDownloads.mockResolvedValue([entry('qwen3:0.6b', 10)]);
		await store.start('qwen3:0.6b');
		await vi.advanceTimersByTimeAsync(500);
		await flush();

		// The backend drops it, but each list call is slower than the poll interval, so a
		// second tick fires while the first is still in flight.
		const pending: ((v: unknown[]) => void)[] = [];
		listActiveDownloads.mockImplementation(
			() =>
				new Promise((resolve) => {
					pending.push(resolve as (v: unknown[]) => void);
				})
		);
		getDownloadProgress.mockResolvedValue({
			progress: 100,
			current_bytes: 1,
			total_bytes: 1,
			completed: true,
			failed: false
		});

		await vi.advanceTimersByTimeAsync(500); // tick 1 starts, blocks
		await vi.advanceTimersByTimeAsync(500); // tick 2 must be skipped by the guard
		expect(pending.length).toBe(1); // proof the second tick never issued a request
		pending.forEach((resolve) => resolve([]));
		await flush();

		expect(toastSuccess).toHaveBeenCalledTimes(1);
	});

	it('hydrates in-flight downloads from the backend after a reload', async () => {
		listActiveDownloads.mockResolvedValue([entry('qwen3:0.6b', 42)]);
		const store = new DownloadsStore();

		await store.hydrate();

		expect(store.activeCount).toBe(1);
		expect(store.get('qwen3:0.6b')?.progress).toBe(42);
		// Nothing was started locally, so no download request should have been made.
		expect(download).not.toHaveBeenCalled();
	});

	it('tracks several concurrent downloads independently', async () => {
		const store = new DownloadsStore();
		listActiveDownloads.mockResolvedValue([entry('qwen3:0.6b', 5), entry('llama3.2:1b', 5)]);
		await store.start('qwen3:0.6b');
		await store.start('llama3.2:1b');
		await vi.advanceTimersByTimeAsync(500);
		await flush();
		expect(store.activeCount).toBe(2);

		// Only one finishes; the other must keep running.
		listActiveDownloads.mockResolvedValue([entry('llama3.2:1b', 30)]);
		getDownloadProgress.mockResolvedValue({
			progress: 100,
			current_bytes: 1,
			total_bytes: 1,
			completed: true,
			failed: false
		});

		await vi.advanceTimersByTimeAsync(500);
		await flush();

		expect(store.isActive('qwen3:0.6b')).toBe(false);
		expect(store.isActive('llama3.2:1b')).toBe(true);
		expect(store.get('llama3.2:1b')?.progress).toBe(30);
	});
});
