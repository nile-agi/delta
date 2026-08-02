import { pushNotification } from '$lib/stores/notifications.svelte';
import { agentService } from './agent';

let pollInterval: ReturnType<typeof setInterval> | undefined;
let nativeSend: ((opts: { title: string; body: string }) => void) | null = null;
let nativeInitDone = false;

async function initNotifications() {
	if (nativeInitDone) return;
	nativeInitDone = true;
	try {
		const {
			isPermissionGranted,
			requestPermission,
			sendNotification
		} = await import('@tauri-apps/plugin-notification');

		let granted = await isPermissionGranted();
		if (!granted) {
			const permission = await requestPermission();
			granted = permission === 'granted';
		}
		if (granted) {
			nativeSend = sendNotification;
		}
	} catch {
		// Tauri plugin not available (e.g. browser dev mode)
	}
}

function notify(
	title: string,
	body: string,
	type: 'event' | 'task',
	eventId?: string,
	time?: string
) {
	if (nativeSend) {
		nativeSend({ title, body });
	}
	pushNotification({ title, body, type, eventId, time });
}

async function checkReminders() {
	try {
		const reminders = await agentService.fetchPendingReminders();
		if (reminders.length === 0) return;

		await initNotifications();

		for (const reminder of reminders) {
			const isEvent = reminder.type === 'event';
			const prefix = isEvent ? '' : 'Task due: ';
			let body = '';

			if (reminder.reminder_minutes > 0) {
				body = `In ${reminder.reminder_minutes} minutes`;
			} else {
				body = isEvent ? 'Starting now' : 'Due now';
			}

			notify(
				`${prefix}${reminder.title}`,
				body,
				reminder.type,
				reminder.id,
				reminder.time
			);
		}
	} catch {
		// Server may not be ready yet
	}
}

export function startReminderPolling() {
	if (pollInterval) return;
	checkReminders();
	pollInterval = setInterval(checkReminders, 30_000);
}

export function stopReminderPolling() {
	if (pollInterval) {
		clearInterval(pollInterval);
		pollInterval = undefined;
	}
}
