import type { CalendarEvent } from '$lib/services/agent';

/** Sentinel for "no reminder". The server's reminder query filters `reminder_minutes >= 0`. */
export const REMINDER_NONE = -1;

export const REMINDER_OPTIONS: Array<{ value: number; label: string }> = [
	{ value: REMINDER_NONE, label: 'No reminder' },
	{ value: 0, label: 'At start time' },
	{ value: 5, label: '5 minutes before' },
	{ value: 15, label: '15 minutes before' },
	{ value: 30, label: '30 minutes before' },
	{ value: 60, label: '1 hour before' },
	{ value: 1440, label: '1 day before' }
];

const pad = (n: number) => String(n).padStart(2, '0');

/** `YYYY-MM-DD` in local time. `toISOString()` would shift the date in any non-UTC zone. */
export function toLocalDateStr(date: Date): string {
	return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`;
}

export function toLocalTimeStr(date: Date): string {
	return `${pad(date.getHours())}:${pad(date.getMinutes())}`;
}

/** Masks free typing into HH:MM, inserting the colon once three digits are in. */
export function handleTimeInput(event: Event, setter: (value: string) => void) {
	const input = event.currentTarget as HTMLInputElement;
	let raw = input.value.replace(/[^0-9]/g, '');
	if (raw.length > 4) raw = raw.slice(0, 4);
	if (raw.length >= 3) raw = raw.slice(0, 2) + ':' + raw.slice(2);
	input.value = raw;
	setter(raw);
}

export function isValidTime(value: string): boolean {
	const match = value.match(/^(\d{2}):(\d{2})$/);
	if (!match) return false;
	return parseInt(match[1]) <= 23 && parseInt(match[2]) <= 59;
}

/** Weekday header rotated so `weekStart` comes first, plus the offset the grid needs. */
export function weekdayNames(weekStart: string): string[] {
	const names = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
	return weekStart === 'sunday' ? names : [...names.slice(1), names[0]];
}

/** Column index (0-6) for a JS day number, honouring the week-start preference. */
export function weekdayColumn(jsDay: number, weekStart: string): number {
	const offset = weekStart === 'sunday' ? 0 : 1;
	return (jsDay - offset + 7) % 7;
}

export function parseTags(raw: string): string[] {
	return raw
		.split(',')
		.map((tag) => tag.trim())
		.filter(Boolean);
}

/** Every distinct tag across the given events, alphabetised. */
export function collectTags(events: CalendarEvent[]): string[] {
	const seen = new Set<string>();
	for (const event of events) for (const tag of parseTags(event.tags ?? '')) seen.add(tag);
	return [...seen].sort((a, b) => a.localeCompare(b));
}

export function formatTime(value: string): string {
	const date = new Date(value);
	if (Number.isNaN(date.getTime())) return '';
	return date.toLocaleTimeString('en-US', { hour: 'numeric', minute: '2-digit' });
}

/** "Today" / "Tomorrow" / "Yesterday" when close enough to be worth saying, else ''. */
export function relativeDayLabel(dateStr: string): string {
	const today = new Date();
	const diff =
		(new Date(dateStr + 'T00:00:00').getTime() -
			new Date(toLocalDateStr(today) + 'T00:00:00').getTime()) /
		86400000;
	if (diff === 0) return 'Today';
	if (diff === 1) return 'Tomorrow';
	if (diff === -1) return 'Yesterday';
	return '';
}

/** All-day items first, then chronological — the order a day is actually experienced. */
export function sortDayItems(items: CalendarEvent[]): CalendarEvent[] {
	return [...items].sort((a, b) => {
		if (a.all_day !== b.all_day) return a.all_day ? -1 : 1;
		return (a.start_time ?? '').localeCompare(b.start_time ?? '');
	});
}
