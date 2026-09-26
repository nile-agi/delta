import { describe, expect, it } from 'vitest';

const { ChatService } = await import('./chat');

describe('ChatService.currentDateLine', () => {
	it('states the real weekday, day, month and year so the model does not guess', () => {
		const line = ChatService.currentDateLine(new Date(2026, 8, 26, 8, 55));
		expect(line).toContain('Saturday');
		expect(line).toContain('26 September 2026');
		expect(line).toContain('08:55');
	});
});
