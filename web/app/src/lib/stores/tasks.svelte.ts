import { agentService, type Task } from '$lib/services/agent';

let tasks = $state<Task[]>([]);
let loading = $state(false);
let filterStatus = $state('');
let filterPriority = $state('');

export function taskList() {
	return tasks;
}

export function tasksLoading() {
	return loading;
}

export function tasksFilterStatus() {
	return filterStatus;
}

export function tasksFilterPriority() {
	return filterPriority;
}

export function setFilterStatus(status: string) {
	filterStatus = status;
}

export function setFilterPriority(priority: string) {
	filterPriority = priority;
}

export async function loadTasks(status?: string, priority?: string) {
	loading = true;
	try {
		tasks = await agentService.listTasks(status, priority);
	} catch (e) {
		console.error('Failed to load tasks:', e);
	} finally {
		loading = false;
	}
}

export async function createTask(data: Partial<Task>) {
	const task = await agentService.createTask(data);
	tasks = [...tasks, task];
	return task;
}

export async function updateTask(id: string, data: Partial<Task>) {
	const task = await agentService.updateTask(id, data);
	tasks = tasks.map((t) => (t.id === id ? task : t));
	return task;
}

export async function completeTask(id: string) {
	const task = await agentService.completeTask(id);
	tasks = tasks.map((t) => (t.id === id ? task : t));
	return task;
}

export async function deleteTask(id: string) {
	await agentService.deleteTask(id);
	tasks = tasks.filter((t) => t.id !== id);
}
