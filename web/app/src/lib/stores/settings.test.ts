import { beforeEach, describe, expect, it, vi } from 'vitest';
const server = vi.hoisted(() => ({ serverDefaultParams: null as Record<string, unknown> | null }));
vi.mock('$app/environment', () => ({ browser: true }));
vi.mock('$lib/stores/server.svelte', () => ({ serverStore: server }));
let storage: Map<string, string>;
beforeEach(() => {
	vi.resetModules();
	server.serverDefaultParams = null;
	storage = new Map();
	vi.stubGlobal('localStorage', {
		getItem: (key: string) => storage.get(key) ?? null,
		setItem: (key: string, value: string) => storage.set(key, value),
		removeItem: (key: string) => storage.delete(key)
	});
});
describe('settings recovery and overrides', () => {
	it('repairs saved invalid DRY values and clears only their overrides', async () => {
		storage.set(
			'config',
			JSON.stringify({
				dry_penalty_last_n: -1,
				top_p: 0.6,
				custom: '{"dry_penalty_last_n":-1,"seed":12}'
			})
		);
		storage.set('userOverrides', JSON.stringify(['dry_penalty_last_n', 'top_p']));
		const { settingsStore: store } = await import('./settings.svelte');
		expect(store.config.dry_penalty_last_n).toBe(64);
		expect(store.config.custom).toBe('{"seed":12}');
		expect(store.userOverrides.has('dry_penalty_last_n')).toBe(false);
		expect(store.userOverrides.has('top_p')).toBe(true);
		expect(store.repairedSettings).toContain('dry_penalty_last_n');
		expect(JSON.parse(storage.get('config')!).dry_penalty_last_n).toBe(64);
	});
	it('rejects invalid updates without changing persisted state', async () => {
		const { settingsStore: store } = await import('./settings.svelte');
		expect(store.updateConfig('dry_penalty_last_n', -1)).toBe(false);
		expect(store.config.dry_penalty_last_n).toBe(64);
	});
	it('uses valid model defaults and preserves an explicit override across model switches', async () => {
		const { settingsStore: store } = await import('./settings.svelte');
		expect(store.updateConfig('top_p', 0.7)).toBe(true);
		server.serverDefaultParams = { top_p: 0.95, dry_penalty_last_n: 128, dry_base: -1 };
		store.syncWithServerDefaults();
		expect(store.config.top_p).toBe(0.7);
		expect(store.config.dry_penalty_last_n).toBe(128);
		expect(store.config.dry_base).toBe(1.75);
		expect(store.updateConfig('theme', 'dark')).toBe(true);
		expect(store.userOverrides.has('dry_penalty_last_n')).toBe(false);
		server.serverDefaultParams = { top_p: 0.8, dry_penalty_last_n: 256 };
		store.syncWithServerDefaults();
		expect(store.config.top_p).toBe(0.7);
		expect(store.config.dry_penalty_last_n).toBe(256);
	});
	it('rolls back when saving overrides fails', async () => {
		const { settingsStore: store } = await import('./settings.svelte');
		store.updateConfig('top_p', 0.7);
		const previous = storage.get('config');
		vi.stubGlobal('localStorage', {
			getItem: (key: string) => storage.get(key) ?? null,
			setItem: (key: string, value: string) => {
				if (key === 'userOverrides') throw new Error('disk full');
				storage.set(key, value);
			},
			removeItem: (key: string) => storage.delete(key)
		});
		expect(store.updateConfig('top_p', 0.6)).toBe(false);
		expect(store.config.top_p).toBe(0.7);
		expect(storage.get('config')).toBe(previous);
	});
});
