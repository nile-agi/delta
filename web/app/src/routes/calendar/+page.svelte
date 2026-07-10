<script lang="ts">
	import { onMount } from 'svelte';
	import { ChevronLeft, ChevronRight, Plus } from '@lucide/svelte';
	import Button from '$lib/components/ui/button/button.svelte';
	import * as Dialog from '$lib/components/ui/dialog';
	import Input from '$lib/components/ui/input/input.svelte';
	import Textarea from '$lib/components/ui/textarea/textarea.svelte';
	import Label from '$lib/components/ui/label/label.svelte';
	import type { CalendarEvent } from '$lib/services/agent';
	import {
		calendarEvents,
		calendarCurrentMonth,
		setCurrentMonth,
		loadMonthEvents,
		createEvent,
		deleteEvent
	} from '$lib/stores/calendar.svelte';

	let showCreateDialog = $state(false);
	let newEvent = $state({ title: '', start_time: '', end_time: '', description: '', location: '' });
	let startDate = $state('');
	let startTime = $state('');
	let endDate = $state('');
	let endTime = $state('');
	let createError = $state('');
	let creating = $state(false);
	let selectedDayEvents = $state<CalendarEvent[]>([]);
	let selectedDate = $state('');

	const month = $derived(calendarCurrentMonth());
	const events = $derived(calendarEvents());

	const daysInMonth = $derived.by(() => {
		const y = month.getFullYear();
		const m = month.getMonth();
		return new Date(y, m + 1, 0).getDate();
	});

	const firstDayOfWeek = $derived.by(() => {
		return new Date(month.getFullYear(), month.getMonth(), 1).getDay();
	});

	const monthLabel = $derived.by(() => {
		return month.toLocaleDateString('en-US', { month: 'long', year: 'numeric' });
	});

	function eventsForDay(day: number): CalendarEvent[] {
		const dateStr = `${month.getFullYear()}-${String(month.getMonth() + 1).padStart(2, '0')}-${String(day).padStart(2, '0')}`;
		return events.filter((e) => e.start_time?.startsWith(dateStr));
	}

	function isToday(day: number): boolean {
		const now = new Date();
		return (
			now.getFullYear() === month.getFullYear() &&
			now.getMonth() === month.getMonth() &&
			now.getDate() === day
		);
	}

	function prevMonth() {
		const d = new Date(month);
		d.setMonth(d.getMonth() - 1);
		setCurrentMonth(d);
		loadMonthEvents(d);
	}

	function nextMonth() {
		const d = new Date(month);
		d.setMonth(d.getMonth() + 1);
		setCurrentMonth(d);
		loadMonthEvents(d);
	}

	function selectDay(day: number) {
		const dateStr = `${month.getFullYear()}-${String(month.getMonth() + 1).padStart(2, '0')}-${String(day).padStart(2, '0')}`;
		selectedDate = dateStr;
		selectedDayEvents = eventsForDay(day);
	}

	async function handleCreateEvent() {
		createError = '';
		if (!newEvent.title) { createError = 'Title is required'; return; }
		if (!startDate) { createError = 'Start date is required'; return; }
		if (!startTime) { createError = 'Start time is required'; return; }
		newEvent.start_time = `${startDate}T${startTime}:00`;
		if (endDate && endTime) {
			newEvent.end_time = `${endDate}T${endTime}:00`;
		}
		creating = true;
		try {
			await createEvent(newEvent);
			newEvent = { title: '', start_time: '', end_time: '', description: '', location: '' };
			startDate = ''; startTime = ''; endDate = ''; endTime = '';
			showCreateDialog = false;
			await loadMonthEvents(month);
		} catch (e) {
			createError = e instanceof Error ? e.message : 'Failed to create event';
			console.error('Failed to create event:', e);
		} finally {
			creating = false;
		}
	}

	async function handleDeleteEvent(id: string) {
		await deleteEvent(id);
		selectedDayEvents = selectedDayEvents.filter((e) => e.id !== id);
	}

	onMount(() => {
		loadMonthEvents(month);

		function onVisible() {
			if (document.visibilityState === 'visible') loadMonthEvents(month);
		}
		document.addEventListener('visibilitychange', onVisible);
		return () => document.removeEventListener('visibilitychange', onVisible);
	});
</script>

<div class="flex h-full flex-col">
	<div class="flex items-center justify-between border-b py-4 pl-14 pr-6">
		<div class="flex items-center gap-4">
			<h1 class="text-2xl font-semibold">Calendar</h1>
			<div class="flex items-center gap-1">
				<Button variant="ghost" size="icon" onclick={prevMonth}>
					<ChevronLeft class="h-4 w-4" />
				</Button>
				<span class="min-w-[160px] text-center text-sm font-medium">{monthLabel}</span>
				<Button variant="ghost" size="icon" onclick={nextMonth}>
					<ChevronRight class="h-4 w-4" />
				</Button>
			</div>
		</div>
		<Button size="sm" onclick={() => {
			const now = new Date();
			const pad = (n: number) => n.toString().padStart(2, '0');
			startDate = `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())}`;
			startTime = `${pad(now.getHours())}:${pad(now.getMinutes())}`;
			endDate = startDate;
			endTime = startTime;
			showCreateDialog = true;
		}}>
			<Plus class="mr-1 h-4 w-4" />
			New Event
		</Button>
	</div>

	<div class="flex flex-1 overflow-hidden">
		<div class="flex-1 overflow-auto p-4">
			<div class="grid grid-cols-7 gap-px rounded-lg border bg-border">
				{#each ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'] as day}
					<div class="bg-muted p-2 text-center text-xs font-medium text-muted-foreground">
						{day}
					</div>
				{/each}

				{#each Array(firstDayOfWeek) as _}
					<div class="min-h-[80px] bg-background p-1"></div>
				{/each}

				{#each Array(daysInMonth) as _, i}
					{@const day = i + 1}
					{@const dayEvents = eventsForDay(day)}
					<button
						class="min-h-[80px] bg-background p-1 text-left transition-colors hover:bg-accent/50"
						class:ring-2={selectedDate ===
							`${month.getFullYear()}-${String(month.getMonth() + 1).padStart(2, '0')}-${String(day).padStart(2, '0')}`}
						class:ring-primary={selectedDate ===
							`${month.getFullYear()}-${String(month.getMonth() + 1).padStart(2, '0')}-${String(day).padStart(2, '0')}`}
						onclick={() => selectDay(day)}
					>
						<span
							class="inline-flex h-6 w-6 items-center justify-center rounded-full text-xs"
							class:bg-primary={isToday(day)}
							class:text-primary-foreground={isToday(day)}
						>
							{day}
						</span>
						{#each dayEvents.slice(0, 2) as event}
							<div
								class="mt-0.5 truncate rounded bg-primary/10 px-1 text-[10px] text-primary"
							>
								{event.title}
							</div>
						{/each}
						{#if dayEvents.length > 2}
							<div class="mt-0.5 text-[10px] text-muted-foreground">
								+{dayEvents.length - 2} more
							</div>
						{/if}
					</button>
				{/each}
			</div>
		</div>

		{#if selectedDate}
			<div class="w-72 border-l p-4">
				<h3 class="mb-3 text-sm font-semibold">
					{new Date(selectedDate + 'T00:00:00').toLocaleDateString('en-US', {
						weekday: 'long',
						month: 'short',
						day: 'numeric'
					})}
				</h3>
				{#if selectedDayEvents.length === 0}
					<p class="text-sm text-muted-foreground">No events</p>
				{:else}
					<div class="space-y-2">
						{#each selectedDayEvents as event}
							<div class="rounded-lg border p-3">
								<div class="flex items-start justify-between">
									<div>
										<p class="text-sm font-medium">{event.title}</p>
										{#if event.start_time}
											<p class="text-xs text-muted-foreground">
												{new Date(event.start_time).toLocaleTimeString('en-US', {
													hour: 'numeric',
													minute: '2-digit'
												})}
											</p>
										{/if}
										{#if event.location}
											<p class="text-xs text-muted-foreground">{event.location}</p>
										{/if}
									</div>
									<Button
										variant="ghost"
										size="sm"
										class="h-6 text-xs text-destructive"
										onclick={() => handleDeleteEvent(event.id)}>Delete</Button
									>
								</div>
								{#if event.description}
									<p class="mt-1 text-xs text-muted-foreground">{event.description}</p>
								{/if}
							</div>
						{/each}
					</div>
				{/if}
			</div>
		{/if}
	</div>
</div>

<Dialog.Root bind:open={showCreateDialog}>
	<Dialog.Content class="sm:max-w-md">
		<Dialog.Header>
			<Dialog.Title>New Event</Dialog.Title>
		</Dialog.Header>
		<div class="space-y-4 py-4">
			<div>
				<Label>Title <span class="text-destructive">*</span></Label>
				<Input bind:value={newEvent.title} placeholder="Event title" class="mt-1" />
			</div>
			<div class="grid grid-cols-2 gap-3">
				<div>
					<Label>Start date <span class="text-destructive">*</span></Label>
					<input
						type="date"
						value={startDate}
						oninput={(e) => { startDate = e.currentTarget.value; }}
						class="mt-1 flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30"
					/>
				</div>
				<div>
					<Label>Start time <span class="text-destructive">*</span></Label>
					<input
						type="time"
						value={startTime}
						oninput={(e) => { startTime = e.currentTarget.value; }}
						class="mt-1 flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30"
					/>
				</div>
			</div>
			<div class="grid grid-cols-2 gap-3">
				<div>
					<Label>End date</Label>
					<input
						type="date"
						value={endDate}
						oninput={(e) => { endDate = e.currentTarget.value; }}
						class="mt-1 flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30"
					/>
				</div>
				<div>
					<Label>End time</Label>
					<input
						type="time"
						value={endTime}
						oninput={(e) => { endTime = e.currentTarget.value; }}
						class="mt-1 flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30"
					/>
				</div>
			</div>
			<div>
				<Label>Location</Label>
				<Input bind:value={newEvent.location} placeholder="Location" class="mt-1" />
			</div>
			<div>
				<Label>Description</Label>
				<Textarea
					bind:value={newEvent.description}
					placeholder="Description"
					class="mt-1 max-h-[120px] resize-none"
					rows={3}
				/>
			</div>
			{#if createError}
				<p class="text-sm text-destructive">{createError}</p>
			{/if}
		</div>
		<Dialog.Footer>
			<Button variant="outline" onclick={() => (showCreateDialog = false)}>Cancel</Button>
			<Button onclick={handleCreateEvent} disabled={creating}>
				{creating ? 'Creating...' : 'Create'}
			</Button>
		</Dialog.Footer>
	</Dialog.Content>
</Dialog.Root>
