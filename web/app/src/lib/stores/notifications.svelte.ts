export interface AppNotification {
	id: string;
	title: string;
	body: string;
	type: 'event' | 'task';
	eventId?: string;
	time?: string;
	timestamp: number;
}

let notifications = $state<AppNotification[]>([]);
let counter = 0;

export function activeNotifications() {
	return notifications;
}

export function pushNotification(n: Omit<AppNotification, 'id' | 'timestamp'>) {
	const id = `notif_${++counter}`;
	notifications = [{ ...n, id, timestamp: Date.now() }, ...notifications];
	return id;
}

export function dismissNotification(id: string) {
	notifications = notifications.filter((n) => n.id !== id);
}

export function dismissAll() {
	notifications = [];
}
