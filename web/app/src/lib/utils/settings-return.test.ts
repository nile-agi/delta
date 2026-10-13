import { describe, expect, it } from 'vitest';
import { settingsReturnPath } from './settings-return';

describe('settingsReturnPath', () => {
	it('returns to the chat that was open', () => {
		expect(settingsReturnPath('conv-7')).toBe('?#/chat/conv-7');
	});

	it('goes home when no chat was open', () => {
		expect(settingsReturnPath(null)).toBe('?#/');
		expect(settingsReturnPath(undefined)).toBe('?#/');
		expect(settingsReturnPath('')).toBe('?#/');
	});

	it('keeps odd ids inside the route', () => {
		expect(settingsReturnPath('a/b c')).toBe('?#/chat/a%2Fb%20c');
	});
});
