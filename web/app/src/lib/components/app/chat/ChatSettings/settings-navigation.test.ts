import { describe, expect, it } from 'vitest';
import { SETTINGS_VIEWS, resolveSettingsLocation } from './settings-navigation';
describe('settings navigation', () => {
	it('offers exactly Info, General and Developer', () => {
		expect(SETTINGS_VIEWS.map((view) => view.title)).toEqual(['Info', 'General', 'Developer']);
	});
	it('opens General preferences by default and maps old links', () => {
		expect(resolveSettingsLocation()).toEqual({ view: 'General', group: 'You' });
		expect(resolveSettingsLocation('Sampling')).toEqual({ view: 'Developer', group: 'Sampling' });
		expect(resolveSettingsLocation('Model Management')).toEqual({
			view: 'General',
			group: 'Model Management'
		});
		expect(resolveSettingsLocation('not-real')).toEqual({ view: 'General', group: 'You' });
	});
});
