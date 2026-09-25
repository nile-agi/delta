import { afterEach, expect, it, vi } from 'vitest';
import { cleanup, render } from 'vitest-browser-svelte';
import { page } from '@vitest/browser/context';

const { openCalendarWindow } = vi.hoisted(() => ({ openCalendarWindow: vi.fn() }));

vi.mock('$lib/services/calendar-window', () => ({ openCalendarWindow }));

import NotificationCenter from './NotificationCenter.svelte';
import {
	activeNotifications,
	dismissAll,
	pushNotification
} from '$lib/stores/notifications.svelte';
import { calendarWindow } from '$lib/stores/calendar-window.svelte';

afterEach(() => {
	cleanup();
	dismissAll();
	openCalendarWindow.mockReset();
	delete (calendarWindow.state as typeof calendarWindow.state & { selectedDate?: string }).selectedDate;
});

it('routes View through the shared calendar service and dismisses the reminder', async () => {
	pushNotification({
		title: 'Task due: Review notes',
		body: 'Due now',
		type: 'task',
		eventId: 'task-1',
		time: '2026-09-21T09:00:00'
	});
	render(NotificationCenter);

	await page.getByRole('button', { name: /Task due: Review notes/ }).click();
	await page.getByRole('button', { name: 'View', exact: true }).click();

	expect(openCalendarWindow).toHaveBeenCalledOnce();
	expect(activeNotifications()).toEqual([]);
	expect('selectedDate' in calendarWindow.state).toBe(false);
});
