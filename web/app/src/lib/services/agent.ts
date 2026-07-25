import { getModelApiBaseUrl } from '$lib/utils/model-api-url';

// Agent API shares the model API's server, so reuse its base URL resolution.
function apiUrl(path: string): string {
	return `${getModelApiBaseUrl()}${path}`;
}

export interface CalendarEvent {
	id: string;
	title: string;
	description: string;
	start_time: string;
	end_time: string;
	location: string;
	all_day: boolean;
	status: 'upcoming' | 'in_progress' | 'completed' | 'cancelled';
	type: 'event' | 'task';
	priority: 'low' | 'medium' | 'high' | 'urgent';
	tags: string;
	reminder_minutes: number;
	reminded: boolean;
	created_at: string;
	updated_at: string;
}

export interface Reminder {
	type: 'event' | 'task';
	id: string;
	title: string;
	time: string;
	reminder_minutes: number;
}

export const agentService = {
	async listEvents(
		start?: string,
		end?: string,
		limit = 50,
		type?: string,
		status?: string,
		priority?: string
	): Promise<CalendarEvent[]> {
		const params = new URLSearchParams();
		if (start) params.set('start', start);
		if (end) params.set('end', end);
		if (type) params.set('type', type);
		if (status) params.set('status', status);
		if (priority) params.set('priority', priority);
		params.set('limit', limit.toString());
		const res = await fetch(apiUrl(`/api/agent/events?${params}`));
		const data = await res.json();
		return data.events ?? [];
	},

	async createEvent(event: Partial<CalendarEvent>): Promise<CalendarEvent> {
		const res = await fetch(apiUrl('/api/agent/events'), {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify(event)
		});
		if (!res.ok) {
			const err = await res.json().catch(() => ({}));
			throw new Error(err.error || `Failed to create event (${res.status})`);
		}
		return res.json();
	},

	async updateEvent(id: string, updates: Partial<CalendarEvent>): Promise<CalendarEvent> {
		const res = await fetch(apiUrl(`/api/agent/events/${id}`), {
			method: 'PUT',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify(updates)
		});
		if (!res.ok) {
			const err = await res.json().catch(() => ({}));
			throw new Error(err.error || `Failed to update event (${res.status})`);
		}
		return res.json();
	},

	async deleteEvent(id: string): Promise<void> {
		await fetch(apiUrl(`/api/agent/events/${id}`), { method: 'DELETE' });
	},

	async fetchPendingReminders(): Promise<Reminder[]> {
		const res = await fetch(apiUrl('/api/agent/reminders/pending'));
		const data = await res.json();
		return data.reminders ?? [];
	},

	async getTools(): Promise<{ tools: unknown[]; tool_names: string[] }> {
		const res = await fetch(apiUrl('/api/agent/tools'));
		return res.json();
	},

	async chat(
		messages: { role: string; content: string }[]
	): Promise<{ content: string; tool_calls_made: number }> {
		const res = await fetch(apiUrl('/v1/chat/completions'), {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify({ messages, stream: false })
		});
		const data = await res.json();
		if (data.error) throw new Error(data.error.message || 'Agent chat failed');
		const choice = data.choices?.[0];
		return {
			content: choice?.message?.content ?? '',
			tool_calls_made: data.tool_calls_made ?? 0
		};
	}
};
