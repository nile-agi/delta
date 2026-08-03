<script lang="ts">
	import { untrack } from 'svelte';
	import * as Dialog from '$lib/components/ui/dialog';
	import Button from '$lib/components/ui/button/button.svelte';
	import Input from '$lib/components/ui/input/input.svelte';
	import Label from '$lib/components/ui/label/label.svelte';
	import Textarea from '$lib/components/ui/textarea/textarea.svelte';
	import type { CalendarEvent } from '$lib/services/agent';
	import {
		REMINDER_NONE,
		REMINDER_OPTIONS,
		handleTimeInput,
		isValidTime,
		toLocalDateStr,
		toLocalTimeStr
	} from '$lib/utils/calendar';

	interface Props {
		open: boolean;
		mode: 'create' | 'edit';
		item?: CalendarEvent | null;
		defaultDate?: string;
		onSubmit: (data: Partial<CalendarEvent>) => Promise<void>;
	}

	let { open = $bindable(), mode, item = null, defaultDate = '', onSubmit }: Props = $props();

	const field =
		'flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30';

	let type = $state<'event' | 'task'>('event');
	let title = $state('');
	let description = $state('');
	let location = $state('');
	let priority = $state('medium');
	let tags = $state('');
	let allDay = $state(false);
	let reminder = $state(15);
	let startDate = $state('');
	let startTime = $state('');
	let endDate = $state('');
	let endTime = $state('');
	let error = $state('');
	let saving = $state(false);

	// Reload only on open, so a reopened edit never shows the last item. untrack keeps the other
	// props out of the dependencies -- reading them would re-run this and wipe the open form.
	$effect(() => {
		if (!open) return;
		untrack(() => {
			error = '';
			if (mode === 'edit' && item) loadFrom(item);
			else loadDefaults();
		});
	});

	function loadFrom(source: CalendarEvent) {
		type = source.type ?? 'event';
		title = source.title;
		description = source.description ?? '';
		location = source.location ?? '';
		priority = source.priority ?? 'medium';
		tags = source.tags ?? '';
		allDay = !!source.all_day;
		reminder = source.reminder_minutes ?? 15;
		[startDate, startTime] = splitStamp(source.start_time);
		[endDate, endTime] = source.end_time ? splitStamp(source.end_time) : ['', ''];
	}

	function loadDefaults() {
		const now = new Date();
		type = 'event';
		title = '';
		description = '';
		location = '';
		priority = 'medium';
		tags = '';
		allDay = false;
		reminder = 15;
		startDate = defaultDate || toLocalDateStr(now);
		startTime = toLocalTimeStr(now);
		endDate = startDate;
		endTime = startTime;
	}

	function splitStamp(stamp: string): [string, string] {
		const [datePart, timePart] = (stamp ?? '').split('T');
		return [datePart ?? '', timePart?.substring(0, 5) ?? ''];
	}

	// Tasks rarely want a reminder; events almost always do.
	function selectType(next: 'event' | 'task') {
		type = next;
		reminder = next === 'task' ? REMINDER_NONE : 15;
	}

	const isTask = $derived(type === 'task');

	const summary = $derived.by(() => {
		if (!title.trim() || !startDate) return '';
		const day = new Date(`${startDate}T00:00:00`);
		if (Number.isNaN(day.getTime())) return '';
		const when = day.toLocaleDateString('en-US', { weekday: 'short', day: 'numeric', month: 'short' });
		let clause = allDay ? `${when}, all day` : `${when}, ${startTime || '--:--'}`;
		if (!allDay && !isTask && endTime && endTime !== startTime) clause += `–${endTime}`;
		const bell = REMINDER_OPTIONS.find((option) => option.value === reminder);
		const tail = reminder === REMINDER_NONE ? '' : ` · reminds ${bell?.label.toLowerCase() ?? ''}`;
		return `${title.trim()} — ${clause}${tail}`;
	});

	async function handleSubmit() {
		error = '';
		if (!title.trim()) {
			error = 'Give it a title.';
			return;
		}
		if (!startDate) {
			error = isTask ? 'Pick a due date.' : 'Pick a start date.';
			return;
		}
		if (!allDay) {
			if (!startTime) {
				error = isTask ? 'Pick a due time.' : 'Pick a start time.';
				return;
			}
			if (!isValidTime(startTime)) {
				error = 'Start time needs to look like HH:MM.';
				return;
			}
			if (endTime && !isValidTime(endTime)) {
				error = 'End time needs to look like HH:MM.';
				return;
			}
		}

		const data: Partial<CalendarEvent> = {
			title: title.trim(),
			description,
			tags,
			all_day: allDay,
			reminder_minutes: reminder,
			start_time: allDay ? `${startDate}T00:00:00` : `${startDate}T${startTime}:00`
		};

		if (mode === 'create') data.type = type;

		if (isTask) {
			data.priority = priority as CalendarEvent['priority'];
		} else {
			data.location = location;
			if (allDay) {
				if (endDate) data.end_time = `${endDate}T23:59:00`;
			} else if (endDate && endTime) {
				data.end_time = `${endDate}T${endTime}:00`;
			}
		}

		saving = true;
		try {
			await onSubmit(data);
			open = false;
		} catch (e) {
			error = e instanceof Error ? e.message : 'Could not save. Try again.';
		} finally {
			saving = false;
		}
	}
</script>

<Dialog.Root bind:open>
	<Dialog.Content class="sm:max-w-lg">
		<Dialog.Header>
			<Dialog.Title>
				{mode === 'edit' ? 'Edit' : 'New'}
				{isTask ? 'task' : 'event'}
			</Dialog.Title>
		</Dialog.Header>

		<div class="space-y-4 py-2">
			{#if mode === 'create'}
				<div class="flex gap-2">
					<button
						type="button"
						class="flex-1 rounded-md border px-3 py-1.5 text-sm transition-colors {!isTask
							? 'border-primary bg-primary/10 text-primary'
							: 'border-input text-muted-foreground hover:bg-accent'}"
						onclick={() => selectType('event')}
					>
						Event
					</button>
					<button
						type="button"
						class="flex-1 rounded-md border px-3 py-1.5 text-sm transition-colors {isTask
							? 'border-orange-500 bg-orange-500/10 text-orange-600 dark:text-orange-400'
							: 'border-input text-muted-foreground hover:bg-accent'}"
						onclick={() => selectType('task')}
					>
						Task
					</button>
				</div>
			{/if}

			<div>
				<Label>Title</Label>
				<Input
					bind:value={title}
					placeholder={isTask ? 'What needs doing?' : "What's happening?"}
					class="mt-1"
				/>
			</div>

			<div class="flex items-center justify-between rounded-md border border-input px-3 py-2">
				<span class="text-sm">All day</span>
				<button
					type="button"
					role="switch"
					aria-checked={allDay}
					aria-label="All day"
					class="relative h-5 w-9 rounded-full transition-colors {allDay
						? 'bg-primary'
						: 'bg-muted-foreground/30'}"
					onclick={() => (allDay = !allDay)}
				>
					<span
						class="absolute top-0.5 h-4 w-4 rounded-full bg-background transition-transform {allDay
							? 'translate-x-4.5'
							: 'translate-x-0.5'}"
					></span>
				</button>
			</div>

			<div class="grid grid-cols-2 gap-3">
				<div>
					<Label>{isTask ? 'Due date' : 'Starts'}</Label>
					<input
						type="date"
						value={startDate}
						oninput={(e) => (startDate = e.currentTarget.value)}
						class="mt-1 {field}"
					/>
				</div>
				{#if !allDay}
					<div>
						<Label>{isTask ? 'Due time' : 'Start time'}</Label>
						<input
							type="text"
							inputmode="numeric"
							placeholder="HH:MM"
							maxlength={5}
							value={startTime}
							oninput={(e) => handleTimeInput(e, (v) => (startTime = v))}
							class="mt-1 {field}"
						/>
					</div>
				{/if}
			</div>

			{#if !isTask}
				<div class="grid grid-cols-2 gap-3">
					<div>
						<Label>Ends</Label>
						<input
							type="date"
							value={endDate}
							oninput={(e) => (endDate = e.currentTarget.value)}
							class="mt-1 {field}"
						/>
					</div>
					{#if !allDay}
						<div>
							<Label>End time</Label>
							<input
								type="text"
								inputmode="numeric"
								placeholder="HH:MM"
								maxlength={5}
								value={endTime}
								oninput={(e) => handleTimeInput(e, (v) => (endTime = v))}
								class="mt-1 {field}"
							/>
						</div>
					{/if}
				</div>

				<div>
					<Label>Location</Label>
					<Input bind:value={location} placeholder="Where" class="mt-1" />
				</div>
			{:else}
				<div>
					<Label>Priority</Label>
					<select
						class="mt-1 {field}"
						value={priority}
						onchange={(e) => (priority = e.currentTarget.value)}
					>
						<option value="low">Low</option>
						<option value="medium">Medium</option>
						<option value="high">High</option>
						<option value="urgent">Urgent</option>
					</select>
				</div>
			{/if}

			<div class="grid grid-cols-2 gap-3">
				<div>
					<Label>Reminder</Label>
					<select
						class="mt-1 {field}"
						value={reminder}
						onchange={(e) => (reminder = Number(e.currentTarget.value))}
					>
						{#each REMINDER_OPTIONS as option (option.value)}
							<option value={option.value}>{option.label}</option>
						{/each}
					</select>
				</div>
				<div>
					<Label>Tags</Label>
					<Input bind:value={tags} placeholder="work, personal" class="mt-1" />
				</div>
			</div>

			<div>
				<Label>Notes</Label>
				<Textarea
					bind:value={description}
					placeholder="Anything worth remembering"
					class="mt-1 max-h-[120px] resize-none"
					rows={3}
				/>
			</div>

			{#if summary}
				<p class="border-l-2 border-primary/40 pl-3 text-xs text-muted-foreground">{summary}</p>
			{/if}
			{#if error}
				<p class="text-sm text-destructive">{error}</p>
			{/if}
		</div>

		<Dialog.Footer>
			<Button variant="outline" onclick={() => (open = false)}>Cancel</Button>
			<Button onclick={handleSubmit} disabled={saving}>
				{#if saving}
					{mode === 'edit' ? 'Saving' : 'Creating'}...
				{:else}
					{mode === 'edit' ? 'Save changes' : 'Create'}
				{/if}
			</Button>
		</Dialog.Footer>
	</Dialog.Content>
</Dialog.Root>
