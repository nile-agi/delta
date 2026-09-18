import { beforeEach, describe, expect, it, vi } from 'vitest';

vi.mock('$app/environment', () => ({ browser: false }));

import { dockStore } from './dock.svelte';
import { settingsWindow } from './settings-window.svelte';

describe('settings window section navigation', () => {
	beforeEach(() => {
		settingsWindow.close();
		settingsWindow.pendingSection = null;
		dockStore.windows = [];
	});

	it('opens a closed window at the requested section', () => {
		settingsWindow.openTo('Model Management');

		expect(settingsWindow.state.open).toBe(true);
		expect(settingsWindow.state.minimized).toBe(false);
		expect(settingsWindow.pendingSection).toBe('Model Management');
	});

	it('restores a minimized window at the requested section', () => {
		settingsWindow.open();
		settingsWindow.setPosition(240, 180);
		settingsWindow.minimize();

		settingsWindow.openTo('Model Management');

		expect(settingsWindow.state.minimized).toBe(false);
		expect(settingsWindow.state.x).toBe(240);
		expect(settingsWindow.state.y).toBe(180);
		expect(dockStore.windows).toEqual([]);
		expect(settingsWindow.pendingSection).toBe('Model Management');
	});
});
