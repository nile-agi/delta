import { agentService, type CalendarEvent } from '$lib/services/agent';

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

export async function loadEvents(start?: string, end?: string) {
	loading = true;
	try {
		events = await agentService.listEvents(start, end);
	} catch (e) {
		console.error('Failed to load events:', e);
	} finally {
		loading = false;
	}
}

export async function loadMonthEvents(date: Date) {
	const year = date.getFullYear();
	const month = date.getMonth();
	const start = new Date(year, month, 1).toISOString().split('T')[0];
	const end = new Date(year, month + 1, 0).toISOString().split('T')[0];
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
