import { agentService, type CalendarEvent } from '$lib/services/agent';
import { toLocalDateStr } from '$lib/utils/calendar';

let events = $state<CalendarEvent[]>([]);
let loading = $state(false);
let currentMonth = $state(new Date());

export function calendarEvents() {
	return events;
}

export function calendarLoading() {
	return loading;
}

export function calendarCurrentMonth() {
	return currentMonth;
}

export function setCurrentMonth(date: Date) {
	currentMonth = date;
}

// A dense month can hold far more than the service's old default of 50.
const MONTH_EVENT_LIMIT = 500;

export async function loadEvents(start?: string, end?: string) {
	loading = true;
	try {
		events = await agentService.listEvents(start, end, MONTH_EVENT_LIMIT);
	} catch (e) {
		console.error('Failed to load events:', e);
	} finally {
		loading = false;
	}
}

export async function loadMonthEvents(date: Date) {
	const year = date.getFullYear();
	const month = date.getMonth();
	// Local formatting, not toISOString() -- that converts to UTC and shifts the range by a day.
	const start = toLocalDateStr(new Date(year, month, 1));
	const end = toLocalDateStr(new Date(year, month + 1, 0));
	await loadEvents(start, end);
}

export async function createEvent(data: Partial<CalendarEvent>) {
	const event = await agentService.createEvent(data);
	events = [...events, event];
	return event;
}

export async function updateEvent(id: string, data: Partial<CalendarEvent>) {
	const event = await agentService.updateEvent(id, data);
	events = events.map((e) => (e.id === id ? event : e));
	return event;
}

export async function deleteEvent(id: string) {
	await agentService.deleteEvent(id);
	events = events.filter((e) => e.id !== id);
}
