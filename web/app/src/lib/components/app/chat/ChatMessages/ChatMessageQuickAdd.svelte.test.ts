import '../../../../../app.css';
import { afterEach, expect, it, vi } from 'vitest';
import { cleanup, render } from 'vitest-browser-svelte';
import { page } from '@vitest/browser/context';
import ChatMessageQuickAdd from './ChatMessageQuickAdd.svelte';
import { agentService } from '$lib/services/agent';
import { setQuickAdd } from '$lib/stores/chat.svelte';
import { toLocalDateStr } from '$lib/utils/calendar';

vi.mock('$lib/services/agent', () => ({ agentService: { createEvent: vi.fn() } }));
vi.mock('$lib/stores/chat.svelte', () => ({ setQuickAdd: vi.fn() }));
vi.mock('$lib/services/calendar-window', () => ({ openCalendarWindow: vi.fn() }));

afterEach(() => {
	cleanup();
	vi.clearAllMocks();
});

it('opens the existing editor instead of saving a past suggestion directly', async () => {
	render(ChatMessageQuickAdd, {
		messageId: 'past',
		suggestion: {
			kind: 'add',
			status: 'offered',
			title: 'Women in AI',
			type: 'event',
			start_time: '2000-01-01T12:00:00',
			all_day: false
		}
	});
	await page.getByRole('button', { name: /Review date/ }).click();
	await expect.element(page.getByRole('heading', { name: 'New event' })).toBeVisible();
	expect(agentService.createEvent).not.toHaveBeenCalled();
});

it.each([false, true])('saves a valid suggestion directly (all day: %s)', async (allDay) => {
	const start = allDay ? `${toLocalDateStr(new Date())}T00:00:00` : '2099-01-01T12:00:00';
	vi.mocked(agentService.createEvent).mockResolvedValue({
		id: 'event-42',
		title: 'Women in AI',
		type: 'event',
		start_time: start,
		all_day: allDay
	} as Awaited<ReturnType<typeof agentService.createEvent>>);
	render(ChatMessageQuickAdd, {
		messageId: 'valid',
		suggestion: {
			kind: 'add',
			status: 'offered',
			title: 'Women in AI',
			type: 'event',
			start_time: start,
			all_day: allDay
		}
	});
	await page.getByRole('button', { name: /Add to calendar/ }).click();
	await expect.poll(() => vi.mocked(setQuickAdd).mock.calls.length).toBe(1);
	expect(agentService.createEvent).toHaveBeenCalledWith(
		expect.objectContaining({ start_time: start })
	);
	expect(setQuickAdd).toHaveBeenCalledWith(
		'valid',
		expect.objectContaining({ status: 'added', event_id: 'event-42' })
	);
});
