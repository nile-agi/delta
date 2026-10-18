import { afterEach, expect, it, vi } from 'vitest';
vi.mock('$lib/stores/settings.svelte', () => ({ config: () => ({}) }));
vi.mock('$lib/utils/model-api-url', () => ({ getModelApiBaseUrl: () => 'http://api.test' }));
const { ModelsService } = await import('./models');
const snapshot = { attempt_id: 'a', state: 'loading', model_name: 'gemma3:270m', model_alias: 'gemma', loaded: false,
	effective_config: { context: 2048, parallel: 1, batch: 64, microbatch: 64, gpu_layers: 0 } };
afterEach(() => vi.unstubAllGlobals());
it('reads exact attempt status and rejects malformed readiness', async () => {
	const fetch = vi.fn().mockResolvedValueOnce(new Response(JSON.stringify(snapshot)))
		.mockResolvedValueOnce(new Response(JSON.stringify({ state: 'ready', loaded: true })));
	vi.stubGlobal('fetch', fetch);
	expect((await ModelsService.getRuntime('a')).state).toBe('loading');
	expect(fetch.mock.calls[0][0]).toBe('http://api.test/api/models/runtime?attempt_id=a');
	await expect(ModelsService.getRuntime('a')).rejects.toThrow('Invalid model runtime response');
});
it('recovers a lost selection response without issuing a duplicate load', async () => {
	const fetch = vi.fn().mockRejectedValueOnce(new TypeError('Failed to fetch'))
		.mockResolvedValueOnce(new Response(JSON.stringify(snapshot)));
	vi.stubGlobal('fetch', fetch);
	expect((await ModelsService.use('gemma3:270m', 2048)).attempt_id).toBe('a');
	expect(fetch.mock.calls.map((c) => c[0])).toEqual(['http://api.test/api/models/use', 'http://api.test/api/models/runtime']);
});
it('does not adopt a different model after a lost response', async () => {
	vi.stubGlobal('fetch', vi.fn().mockRejectedValueOnce(new TypeError('Failed to fetch'))
		.mockResolvedValueOnce(new Response(JSON.stringify(snapshot))));
	await expect(ModelsService.use('other')).rejects.toThrow();
});
it('cancels the exact attempt', async () => {
	const fetch = vi.fn().mockResolvedValue(new Response(JSON.stringify({ ...snapshot, state: 'cancelled' })));
	vi.stubGlobal('fetch', fetch);
	expect((await ModelsService.cancelRuntime('a')).state).toBe('cancelled');
	expect(JSON.parse(fetch.mock.calls[0][1].body)).toEqual({ attempt_id: 'a' });
});
it('can retry a smaller context without overwriting saved preferences', async () => {
	const fetch = vi.fn().mockResolvedValue(new Response(JSON.stringify(snapshot)));
	vi.stubGlobal('fetch', fetch);
	await ModelsService.use('gemma3:270m', 1024, false);
	expect(JSON.parse(fetch.mock.calls[0][1].body)).toEqual({ model: 'gemma3:270m', ctx_size: 1024, persist_ctx: false });
});
