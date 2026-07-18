import { agentService } from './agent';

let pollInterval: ReturnType<typeof setInterval> | undefined;
let notificationSupported = false;

async function initNotifications() {
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
		notificationSupported = granted;
		return { sendNotification };
	} catch {
		notificationSupported = false;
		return null;
	}
}

async function checkReminders() {
	try {
		const reminders = await agentService.fetchPendingReminders();
		if (reminders.length === 0) return;

		const notif = await initNotifications();

		for (const reminder of reminders) {
			const isEvent = reminder.type === 'event';
			const prefix = isEvent ? '' : 'Task due: ';
			let body = '';

			if (reminder.reminder_minutes > 0) {
				body = `In ${reminder.reminder_minutes} minutes`;
			} else {
				body = isEvent ? 'Starting now' : 'Due now';
			}

			if (notif?.sendNotification) {
				notif.sendNotification({
					title: `${prefix}${reminder.title}`,
					body
				});
			}
		}
	} catch {
		// Server may not be ready yet — silently ignore
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
