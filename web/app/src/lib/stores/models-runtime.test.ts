import { afterEach, beforeEach, expect, it, vi } from 'vitest';
const api = vi.hoisted(() => ({ list: vi.fn(), listInstalled: vi.fn(), use: vi.fn(), getRuntime: vi.fn(), cancelRuntime: vi.fn() }));
const context = vi.hoisted(() => vi.fn());
vi.mock('$lib/services/models', () => ({ ModelsService: api }));
vi.mock('$lib/services/slots', () => ({ slotsService: { setLoadedContextTotal: context } }));
vi.mock('$lib/stores/persisted.svelte', () => ({ persisted: () => ({ value: null }) }));
vi.mock('$lib/stores/settings.svelte', () => ({ config: () => ({}) }));
vi.mock('$lib/stores/server.svelte', () => ({ serverSupportsTools: () => false }));
vi.mock('$lib/utils/model-api-url', () => ({ forceModelApiSeparatePort: vi.fn() }));
const { ModelsStore } = await import('./models.svelte');
const status = (id: string, state = 'loading') => ({ attempt_id: id, state, model_name: 'gemma3:270m', model_alias: 'gemma', loaded: state === 'ready',
	effective_config: { context: 2048, parallel: 1, batch: 64, microbatch: 64, gpu_layers: 0 } });
beforeEach(() => {
	vi.useFakeTimers(); vi.stubGlobal('window', {}); vi.stubGlobal('localStorage', { getItem: () => null });
	api.list.mockResolvedValue({ data: [] });
	api.listInstalled.mockResolvedValue({ models: [{ name: 'gemma3:270m', display_name: 'Gemma' }] });
	api.use.mockResolvedValue({ ...status('a'), success: true });
	api.getRuntime.mockResolvedValue(status('a')); context.mockClear();
});
afterEach(() => { vi.clearAllTimers(); vi.useRealTimers(); vi.unstubAllGlobals(); vi.clearAllMocks(); });
it('keeps chat disabled until the worker is ready and uses verified context', async () => {
	const store = new ModelsStore(); await store.fetch(); await store.select('gemma3:270m');
	expect(store.modelLoadedOnServer).toBe(false);
	api.getRuntime.mockResolvedValue(status('a', 'ready'));
	await vi.advanceTimersByTimeAsync(1000);
	expect(store.modelLoadedOnServer).toBe(true); expect(context).toHaveBeenCalledWith(2048);
});
it('resumes a running attempt after reloading the UI', async () => {
	const store = new ModelsStore(); await store.fetch();
	expect(store.attemptId).toBe('a'); expect(store.updating).toBe(true);
	expect(store.selectedModelId).toBe('gemma3:270m');
	expect(store.modelLoadedOnServer).toBe(false);
});
it('ignores a late response from an earlier attempt for the same model', async () => {
	const store = new ModelsStore(); await store.fetch();
	let complete!: (value: unknown) => void;
	api.use.mockImplementationOnce(() => new Promise((resolve) => { complete = resolve; }));
	const first = store.select('gemma3:270m');
	api.use.mockResolvedValueOnce({ ...status('b'), success: true });
	await store.select('gemma3:270m'); complete({ ...status('a', 'ready'), success: true }); await first;
	expect(store.attemptId).toBe('b'); expect(store.modelLoadedOnServer).toBe(false);
});
it('keeps chat disabled after connection loss, failure and cancellation', async () => {
	const store = new ModelsStore(); await store.fetch(); await store.select('gemma3:270m');
	api.getRuntime.mockRejectedValueOnce(new Error('offline'));
	await vi.advanceTimersByTimeAsync(1000); expect(store.modelLoadedOnServer).toBe(false);
	api.getRuntime.mockResolvedValue({ ...status('a', 'failed'), failure: { code: 'worker_exit', message: 'Worker exited', exit_code: 1 } });
	await vi.advanceTimersByTimeAsync(1000);
	expect(store.error).toBe('Worker exited'); expect(store.updating).toBe(false);
	await store.select('gemma3:270m');
	api.cancelRuntime.mockResolvedValue(status('a', 'cancelled')); await store.cancelLoad();
	expect(store.modelLoadedOnServer).toBe(false); expect(store.updating).toBe(false);
});
