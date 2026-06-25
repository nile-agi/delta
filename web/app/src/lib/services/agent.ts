import { getModelApiBaseUrl } from '$lib/utils/model-api-url';

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
	created_at: string;
	updated_at: string;
}

export interface Task {
	id: string;
	title: string;
	description: string;
	status: 'pending' | 'in_progress' | 'completed' | 'cancelled';
	priority: 'low' | 'medium' | 'high' | 'urgent';
	due_date: string;
	tags: string;
	created_at: string;
	updated_at: string;
}

export const agentService = {
	async listEvents(start?: string, end?: string, limit = 50): Promise<CalendarEvent[]> {
		const params = new URLSearchParams();
		if (start) params.set('start', start);
		if (end) params.set('end', end);
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
		return res.json();
	},

	async updateEvent(id: string, updates: Partial<CalendarEvent>): Promise<CalendarEvent> {
		const res = await fetch(apiUrl(`/api/agent/events/${id}`), {
			method: 'PUT',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify(updates)
		});
		return res.json();
	},

	async deleteEvent(id: string): Promise<void> {
		await fetch(apiUrl(`/api/agent/events/${id}`), { method: 'DELETE' });
	},

	async listTasks(
		status?: string,
		priority?: string,
		tags?: string,
		limit = 50
	): Promise<Task[]> {
		const params = new URLSearchParams();
		if (status) params.set('status', status);
		if (priority) params.set('priority', priority);
		if (tags) params.set('tags', tags);
		params.set('limit', limit.toString());
		const res = await fetch(apiUrl(`/api/agent/tasks?${params}`));
		const data = await res.json();
		return data.tasks ?? [];
	},

	async createTask(task: Partial<Task>): Promise<Task> {
		const res = await fetch(apiUrl('/api/agent/tasks'), {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify(task)
		});
		return res.json();
	},

	async updateTask(id: string, updates: Partial<Task>): Promise<Task> {
		const res = await fetch(apiUrl(`/api/agent/tasks/${id}`), {
			method: 'PUT',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify(updates)
		});
		return res.json();
	},

	async completeTask(id: string): Promise<Task> {
		const res = await fetch(apiUrl(`/api/agent/tasks/${id}/complete`), {
			method: 'POST'
		});
		return res.json();
	},

	async deleteTask(id: string): Promise<void> {
		await fetch(apiUrl(`/api/agent/tasks/${id}`), { method: 'DELETE' });
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
