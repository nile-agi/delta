<script lang="ts">
	import { CalendarCheck, CalendarPlus, ListTodo, Pencil, X } from '@lucide/svelte';
	import EventFormDialog from '$lib/components/app/calendar/EventFormDialog.svelte';
	import { agentService, type CalendarEvent } from '$lib/services/agent';
	import { openCalendarWindow } from '$lib/services/calendar-window';
	import { setQuickAdd } from '$lib/stores/chat.svelte';
	import type { QuickAddSuggestion } from '$lib/types/agent';
	import { REMINDER_NONE } from '$lib/utils/calendar';
	import { toast } from 'svelte-sonner';

	interface Props {
		messageId: string;
		suggestion: QuickAddSuggestion;
	}

	let { messageId, suggestion }: Props = $props();

	let saving = $state(false);
	let editing = $state(false);

	const when = $derived(describeWhen(suggestion.start_time, suggestion.all_day));
	const isTask = $derived(suggestion.type === 'task');
	// The suggestion's own `status` is chip state, not the calendar's, so only the item fields go in.
	const draft = $derived<Partial<CalendarEvent>>({
		title: suggestion.title,
		type: suggestion.type,
		start_time: suggestion.start_time,
		all_day: suggestion.all_day
	});

	function describeWhen(stamp: string, allDay: boolean): string {
		const [datePart, timePart] = stamp.split('T');
		const [y, m, d] = datePart.split('-').map(Number);
		const date = new Date(y, m - 1, d);
		const today = new Date();
		today.setHours(0, 0, 0, 0);
		const days = Math.round((date.getTime() - today.getTime()) / 86_400_000);
		const day =
			days === 0
				? 'Today'
				: days === 1
					? 'Tomorrow'
					: date.toLocaleDateString(undefined, {
							weekday: 'short',
							day: 'numeric',
							month: 'short'
						});
		return allDay ? `${day}, all day` : `${day} ${timePart.substring(0, 5)}`;
	}

	async function add(data: Partial<CalendarEvent>) {
		saving = true;
		try {
			const created = await agentService.createEvent(data);
			await setQuickAdd(messageId, {
				title: created.title,
				type: created.type,
				start_time: created.start_time,
				all_day: created.all_day,
				status: 'added',
				event_id: created.id
			});
		} catch (error) {
			toast.error(error instanceof Error ? error.message : 'Could not add it to the calendar');
		} finally {
			saving = false;
		}
	}

	function addAsSuggested() {
		void add({ ...draft, reminder_minutes: isTask ? REMINDER_NONE : 15 });
	}

	function dismiss() {
		void setQuickAdd(messageId, { ...suggestion, status: 'dismissed' });
	}
</script>

{#if suggestion.status === 'offered'}
	<div
		class="flex max-w-[80%] items-center gap-1 rounded-full border border-border/70 bg-muted/40 p-0.5 pl-1 text-xs text-muted-foreground"
	>
		<button
			class="flex min-w-0 items-center gap-1.5 rounded-full px-2 py-1 text-left transition-colors hover:bg-accent hover:text-accent-foreground disabled:opacity-60"
			disabled={saving}
			onclick={addAsSuggested}
			title={isTask ? 'Add this task to your calendar' : 'Add this event to your calendar'}
		>
			{#if isTask}
				<ListTodo class="h-3.5 w-3.5 shrink-0" />
			{:else}
				<CalendarPlus class="h-3.5 w-3.5 shrink-0" />
			{/if}
			<span class="shrink-0 font-medium text-foreground">{isTask ? 'Add task' : 'Add to calendar'}</span>
			<span class="truncate">· {suggestion.title} · {when}</span>
		</button>
		<button
			class="rounded-full p-1.5 transition-colors hover:bg-accent hover:text-accent-foreground"
			aria-label="Edit before adding"
			title="Edit before adding"
			onclick={() => (editing = true)}
		>
			<Pencil class="h-3 w-3" />
		</button>
		<button
			class="rounded-full p-1.5 transition-colors hover:bg-accent hover:text-accent-foreground"
			aria-label="Dismiss suggestion"
			title="Dismiss"
			onclick={dismiss}
		>
			<X class="h-3 w-3" />
		</button>
	</div>

	<EventFormDialog bind:open={editing} mode="create" {draft} onSubmit={add} />
{:else if suggestion.status === 'added'}
	<div class="flex max-w-[80%] items-center gap-1.5 px-1 text-xs text-muted-foreground">
		<CalendarCheck class="h-3.5 w-3.5 shrink-0 text-emerald-500" />
		<span class="truncate">Added · {suggestion.title} · {when}</span>
		<button class="shrink-0 underline-offset-2 hover:text-foreground hover:underline" onclick={openCalendarWindow}>
			Open
		</button>
	</div>
{/if}
