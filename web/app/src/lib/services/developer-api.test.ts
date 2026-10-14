import { afterEach, describe, expect, it, vi } from 'vitest';

const settings = vi.hoisted(() => ({ apiKey: '' }));
const updateConfig = vi.hoisted(() =>
	vi.fn((key: string, value: string) => {
		(settings as Record<string, string>)[key] = value;
	})
);
vi.mock('$lib/stores/settings.svelte', () => ({ config: () => settings, updateConfig }));
vi.mock('$lib/utils/model-api-url', () => ({ getModelApiBaseUrl: () => 'http://api.test' }));

const { apiSnippets, listApiModels, syncApiKey, updateApiServer } = await import('./developer-api');

const state = (overrides: Record<string, unknown> = {}) => ({
	network: false,
	host: '127.0.0.1',
	port: 8081,
	local_url: 'http://127.0.0.1:8081/v1',
	network_url: null,
	has_key: false,
	api_key: '',
	overridden: false,
	...overrides
});

afterEach(() => {
	vi.unstubAllGlobals();
	settings.apiKey = '';
	updateConfig.mockClear();
});

describe('developer API service', () => {
	it('gives Delta its own chats the key the API now requires', async () => {
		vi.stubGlobal(
			'fetch',
			vi.fn(async () => new Response(JSON.stringify(state({ has_key: true, api_key: 'delta-k1' }))))
		);
		await updateApiServer({ key: 'generate' });
		expect(settings.apiKey).toBe('delta-k1');
	});

	it('picks up a key set from the command line at startup', async () => {
		vi.stubGlobal(
			'fetch',
			vi.fn(async () => new Response(JSON.stringify(state({ has_key: true, api_key: 'delta-cli' }))))
		);
		await syncApiKey();
		expect(settings.apiKey).toBe('delta-cli');
	});

	it('clears a stale key and ignores an engine without the route', async () => {
		settings.apiKey = 'old';
		vi.stubGlobal('fetch', vi.fn(async () => new Response(JSON.stringify(state()))));
		await syncApiKey();
		expect(settings.apiKey).toBe('');
		vi.stubGlobal('fetch', vi.fn(async () => new Response('{}', { status: 404 })));
		await expect(syncApiKey()).resolves.toBeUndefined();
	});

	it('lists model ids with the key', async () => {
		const fetch = vi.fn(
			async () => new Response(JSON.stringify({ data: [{ id: 'qwen2.5:0.5b' }, { id: 'gemma' }] }))
		);
		vi.stubGlobal('fetch', fetch);
		expect(await listApiModels('delta-k1')).toEqual(['qwen2.5:0.5b', 'gemma']);
		expect(fetch).toHaveBeenCalledWith('http://api.test/v1/models', {
			headers: { Authorization: 'Bearer delta-k1' }
		});
	});

	it('writes examples for the live address, model and key', () => {
		const open = apiSnippets('http://127.0.0.1:8081/v1', 'qwen2.5:0.5b', false);
		expect(open.curl).toContain('http://127.0.0.1:8081/v1/chat/completions');
		expect(open.curl).not.toContain('Authorization');
		expect(open.python).toContain('base_url="http://127.0.0.1:8081/v1"');
		expect(open.javascript).toContain('model: "qwen2.5:0.5b"');
		const keyed = apiSnippets('http://127.0.0.1:8081/v1', 'm', true);
		expect(keyed.curl).toContain('Authorization: Bearer YOUR_API_KEY');
		expect(keyed.python).toContain('api_key="YOUR_API_KEY"');
	});
});
