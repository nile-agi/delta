import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { CalendarEvent } from '$lib/services/agent';

const { listEvents } = vi.hoisted(() => ({ listEvents: vi.fn() }));

vi.mock('$lib/services/agent', () => ({
	agentService: { listEvents }
}));

function deferred<T>() {
	let resolve!: (value: T) => void;
	const promise = new Promise<T>((done) => {
		resolve = done;
	});
	return { promise, resolve };
}

function event(id: string, start_time: string): CalendarEvent {
	return {
		id,
		title: id,
		description: '',
		start_time,
		end_time: start_time,
		location: '',
		all_day: true,
		status: 'upcoming',
		type: 'event',
		priority: 'low',
		tags: '',
		reminder_minutes: 0,
		reminded: false,
		created_at: start_time,
		updated_at: start_time
	};
}

describe('calendar loading', () => {
	beforeEach(() => {
		vi.resetModules();
		listEvents.mockReset();
	});

	it('keeps the newest month when an older request finishes last', async () => {
		const january = deferred<CalendarEvent[]>();
		const february = deferred<CalendarEvent[]>();
		listEvents.mockReturnValueOnce(january.promise).mockReturnValueOnce(february.promise);
		const calendar = await import('./calendar.svelte');

		const olderLoad = calendar.loadMonthEvents(new Date(2026, 0, 1));
		const newerLoad = calendar.loadMonthEvents(new Date(2026, 1, 1));
		february.resolve([event('February event', '2026-02-03T09:00:00')]);
		await newerLoad;
		january.resolve([event('January event', '2026-01-03T09:00:00')]);
		await olderLoad;

		expect(calendar.calendarEvents().map((item) => item.id)).toEqual(['February event']);
	});
});
