import { afterEach, expect, it, vi } from 'vitest';
import { cleanup, render } from 'vitest-browser-svelte';
import { page } from '@vitest/browser/context';
import type { CalendarEvent } from '$lib/services/agent';

const { listEvents } = vi.hoisted(() => ({ listEvents: vi.fn() }));

vi.mock('$lib/services/agent', () => ({
	agentService: { listEvents }
}));

import Calendar from './Calendar.svelte';
import { loadMonthEvents } from '$lib/stores/calendar.svelte';

function deferred<T>() {
	let resolve!: (value: T) => void;
	const promise = new Promise<T>((done) => {
		resolve = done;
	});
	return { promise, resolve };
}

afterEach(() => {
	cleanup();
	vi.restoreAllMocks();
});

it('uses a centered first-load state and preserves the calendar during refreshes', async () => {
	const initial = deferred<CalendarEvent[]>();
	listEvents.mockReturnValue(initial.promise);
	render(Calendar);

	await expect.element(page.getByRole('status', { name: 'Loading calendar' })).toBeVisible();

	initial.resolve([]);
	await expect
		.element(page.getByRole('status', { name: 'Loading calendar' }))
		.not.toBeInTheDocument();
	await expect.element(page.getByRole('button', { name: 'Today' })).toBeVisible();

	const refresh = deferred<CalendarEvent[]>();
	listEvents.mockReturnValue(refresh.promise);
	const refreshPromise = loadMonthEvents(new Date());

	await expect.element(page.getByRole('status', { name: 'Refreshing calendar' })).toBeVisible();
	await expect.element(page.getByRole('button', { name: 'Today' })).toBeVisible();
	await expect
		.element(page.getByRole('status', { name: 'Loading calendar' }))
		.not.toBeInTheDocument();

	refresh.resolve([]);
	await refreshPromise;
	await expect
		.element(page.getByRole('status', { name: 'Refreshing calendar' }))
		.not.toBeInTheDocument();
});
