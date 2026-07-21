<script lang="ts">
	import { onMount } from 'svelte';
	import { ChevronLeft, ChevronRight, Plus, Check, X, Undo2, Trash2, MapPin, Clock, Play, Pencil } from '@lucide/svelte';
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
		updateEvent,
		deleteEvent
	} from '$lib/stores/calendar.svelte';

	let showCreateDialog = $state(false);
	let newEventType = $state<'event' | 'task'>('event');
	let newTitle = $state('');
	let newDescription = $state('');
	let newLocation = $state('');
	let newPriority = $state('medium');
	let startDate = $state('');
	let startTime = $state('');
	let endDate = $state('');
	let endTime = $state('');
	let createError = $state('');
	let creating = $state(false);
	let selectedDate = $state('');

	let showEditDialog = $state(false);
	let editingItem = $state<CalendarEvent | null>(null);
	let editTitle = $state('');
	let editDescription = $state('');
	let editLocation = $state('');
	let editPriority = $state('medium');
	let editStartDate = $state('');
	let editStartTime = $state('');
	let editEndDate = $state('');
	let editEndTime = $state('');
	let editError = $state('');
	let saving = $state(false);

	function handleTimeInput(e: Event, setter: (v: string) => void) {
		const input = e.currentTarget as HTMLInputElement;
		let raw = input.value.replace(/[^0-9]/g, '');
		if (raw.length > 4) raw = raw.slice(0, 4);
		if (raw.length >= 3) {
			raw = raw.slice(0, 2) + ':' + raw.slice(2);
		}
		input.value = raw;
		setter(raw);
	}

	function isValidTime(v: string): boolean {
		const m = v.match(/^(\d{1,2}):(\d{2})$/);
		if (!m) return false;
		const h = parseInt(m[1]), min = parseInt(m[2]);
		return h >= 0 && h <= 23 && min >= 0 && min <= 59;
	}

	const month = $derived(calendarCurrentMonth());
	const events = $derived(calendarEvents());

	const selectedDayItems = $derived.by(() => {
		if (!selectedDate) return [];
		return events.filter((e) => e.start_time?.startsWith(selectedDate));
	});

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

	function itemsForDay(day: number): CalendarEvent[] {
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
	}

	function resetCreateForm() {
		newEventType = 'event';
		newTitle = '';
		newDescription = '';
		newLocation = '';
		newPriority = 'medium';
		startDate = '';
		startTime = '';
		endDate = '';
		endTime = '';
		createError = '';
	}

	function openCreateDialog() {
		const now = new Date();
		const pad = (n: number) => n.toString().padStart(2, '0');
		resetCreateForm();
		startDate = `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())}`;
		startTime = `${pad(now.getHours())}:${pad(now.getMinutes())}`;
		endDate = startDate;
		endTime = startTime;
		showCreateDialog = true;
	}

	async function handleCreateEvent() {
		createError = '';
		if (!newTitle) { createError = 'Title is required'; return; }
		if (!startDate) { createError = 'Start date is required'; return; }
		if (!startTime) { createError = 'Start time is required'; return; }
		if (!isValidTime(startTime)) { createError = 'Invalid time format (use HH:MM)'; return; }
		if (endTime && !isValidTime(endTime)) { createError = 'Invalid end time format (use HH:MM)'; return; }

		const data: Partial<CalendarEvent> = {
			title: newTitle,
			start_time: `${startDate}T${startTime}:00`,
			description: newDescription,
			type: newEventType
		};

		if (newEventType === 'event') {
			data.location = newLocation;
			if (endDate && endTime) {
				data.end_time = `${endDate}T${endTime}:00`;
			}
		} else {
			data.priority = newPriority as CalendarEvent['priority'];
		}

		creating = true;
		try {
			await createEvent(data);
			resetCreateForm();
			showCreateDialog = false;
			await loadMonthEvents(month);
		} catch (e) {
			createError = e instanceof Error ? e.message : 'Failed to create';
			console.error('Failed to create:', e);
		} finally {
			creating = false;
		}
	}

	async function handleMarkStatus(id: string, status: 'completed' | 'cancelled' | 'upcoming' | 'in_progress') {
		await updateEvent(id, { status } as Partial<CalendarEvent>);
		await loadMonthEvents(month);
	}

	async function handleDeleteItem(id: string) {
		await deleteEvent(id);
		await loadMonthEvents(month);
	}

	function openEditDialog(item: CalendarEvent) {
		editingItem = item;
		editTitle = item.title;
		editDescription = item.description || '';
		editLocation = item.location || '';
		editPriority = item.priority || 'medium';
		if (item.start_time) {
			const [d, t] = item.start_time.split('T');
			editStartDate = d;
			editStartTime = t?.substring(0, 5) || '';
		}
		if (item.end_time) {
			const [d, t] = item.end_time.split('T');
			editEndDate = d;
			editEndTime = t?.substring(0, 5) || '';
		} else {
			editEndDate = '';
			editEndTime = '';
		}
		editError = '';
		showEditDialog = true;
	}

	async function handleSaveEdit() {
		if (!editingItem) return;
		editError = '';
		if (!editTitle) { editError = 'Title is required'; return; }
		if (!editStartDate || !editStartTime) { editError = 'Date and time are required'; return; }
		if (!isValidTime(editStartTime)) { editError = 'Invalid time format (use HH:MM)'; return; }
		if (editEndTime && !isValidTime(editEndTime)) { editError = 'Invalid end time format (use HH:MM)'; return; }

		const updates: Partial<CalendarEvent> = {
			title: editTitle,
			start_time: `${editStartDate}T${editStartTime}:00`,
			description: editDescription,
			location: editLocation
		};

		if (editingItem.type === 'task') {
			updates.priority = editPriority as CalendarEvent['priority'];
		}
		if (editingItem.type === 'event' && editEndDate && editEndTime) {
			updates.end_time = `${editEndDate}T${editEndTime}:00`;
		}

		saving = true;
		try {
			await updateEvent(editingItem.id, updates);
			showEditDialog = false;
			editingItem = null;
			await loadMonthEvents(month);
		} catch (e) {
			editError = e instanceof Error ? e.message : 'Failed to save';
		} finally {
			saving = false;
		}
	}

	onMount(() => {
		loadMonthEvents(month);

		function onVisible() {
			if (document.visibilityState === 'visible') loadMonthEvents(month);
		}
		document.addEventListener('visibilitychange', onVisible);

		const refreshInterval = setInterval(() => {
			if (document.visibilityState === 'visible') loadMonthEvents(month);
		}, 10000);

		return () => {
			document.removeEventListener('visibilitychange', onVisible);
			clearInterval(refreshInterval);
		};
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
		<Button size="sm" onclick={openCreateDialog}>
			<Plus class="mr-1 h-4 w-4" />
			New
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
					{@const dayItems = itemsForDay(day)}
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
						{#each dayItems.slice(0, 2) as item}
							<div
								class="mt-0.5 truncate rounded px-1 text-[10px] {item.type === 'task' ? 'bg-orange-500/10 text-orange-600 dark:text-orange-400' : 'bg-primary/10 text-primary'} {item.status === 'completed' ? 'line-through opacity-50' : ''} {item.status === 'cancelled' ? 'line-through opacity-30' : ''}"
							>
								{item.title}
							</div>
						{/each}
						{#if dayItems.length > 2}
							<div class="mt-0.5 text-[10px] text-muted-foreground">
								+{dayItems.length - 2} more
							</div>
						{/if}
					</button>
				{/each}
			</div>
		</div>

		{#if selectedDate}
			<div class="w-72 overflow-y-auto border-l p-4">
				<h3 class="mb-3 text-sm font-semibold">
					{new Date(selectedDate + 'T00:00:00').toLocaleDateString('en-US', {
						weekday: 'long',
						month: 'short',
						day: 'numeric'
					})}
				</h3>
				{#if selectedDayItems.length === 0}
					<p class="text-sm text-muted-foreground">Nothing scheduled</p>
				{:else}
					<div class="space-y-2">
						{#each selectedDayItems as item (item.id)}
							<div class="rounded-lg border p-3 {item.type === 'task' ? 'border-orange-500/30' : ''} {item.status === 'completed' ? 'opacity-60' : ''} {item.status === 'cancelled' ? 'opacity-40' : ''}">
								<div class="flex items-start gap-2">
									{#if item.status === 'completed'}
										<button
											class="mt-0.5 flex h-5 w-5 shrink-0 items-center justify-center rounded-full bg-green-500 text-white"
											title="Mark as upcoming"
											onclick={() => handleMarkStatus(item.id, 'upcoming')}
										>
											<Check class="h-3 w-3" />
										</button>
									{:else if item.status === 'cancelled'}
										<button
											class="mt-0.5 flex h-5 w-5 shrink-0 items-center justify-center rounded-full bg-muted text-muted-foreground"
											title="Restore"
											onclick={() => handleMarkStatus(item.id, 'upcoming')}
										>
											<Undo2 class="h-3 w-3" />
										</button>
									{:else if item.status === 'in_progress'}
										<button
											class="mt-0.5 flex h-5 w-5 shrink-0 items-center justify-center rounded-full bg-blue-500 text-white"
											title="Mark as done"
											onclick={() => handleMarkStatus(item.id, 'completed')}
										>
											<Play class="h-2.5 w-2.5" />
										</button>
									{:else}
										<button
											class="mt-0.5 flex h-5 w-5 shrink-0 items-center justify-center rounded-full border-2 {item.type === 'task' ? 'border-orange-500/30 hover:border-green-500 hover:bg-green-500/10' : 'border-muted-foreground/30 hover:border-green-500 hover:bg-green-500/10'}"
											title="Mark as done"
											onclick={() => handleMarkStatus(item.id, 'completed')}
										>
										</button>
									{/if}
									<div class="min-w-0 flex-1">
										<div class="flex items-center gap-1.5">
											{#if item.type === 'task'}
												<span class="inline-block h-2 w-2 shrink-0 rounded-full bg-orange-500"></span>
											{/if}
											<p class="text-sm font-medium {item.status === 'completed' || item.status === 'cancelled' ? 'line-through' : ''}">{item.title}</p>
										</div>
										{#if item.start_time}
											<div class="mt-0.5 flex items-center gap-1 text-xs text-muted-foreground">
												<Clock class="h-3 w-3" />
												{new Date(item.start_time).toLocaleTimeString('en-US', {
													hour: 'numeric',
													minute: '2-digit'
												})}{#if item.end_time}
													<span class="text-muted-foreground/60">-</span>
													{new Date(item.end_time).toLocaleTimeString('en-US', {
														hour: 'numeric',
														minute: '2-digit'
													})}
												{/if}
											</div>
										{/if}
										{#if item.location}
											<div class="mt-0.5 flex items-center gap-1 text-xs text-muted-foreground">
												<MapPin class="h-3 w-3" />
												{item.location}
											</div>
										{/if}
										{#if item.type === 'task' && item.priority && item.priority !== 'medium'}
											<span class="mt-0.5 inline-block rounded px-1 text-[10px] font-medium {item.priority === 'urgent' ? 'bg-red-500/10 text-red-600' : item.priority === 'high' ? 'bg-orange-500/10 text-orange-600' : 'bg-muted text-muted-foreground'}">
												{item.priority}
											</span>
										{/if}
										{#if item.status === 'in_progress'}
											<span class="mt-0.5 inline-block rounded bg-blue-500/10 px-1 text-[10px] font-medium text-blue-600 dark:text-blue-400">
												in progress
											</span>
										{/if}
										{#if item.description}
											<p class="mt-1.5 rounded bg-muted/50 px-2 py-1 text-xs text-muted-foreground">{item.description}</p>
										{/if}
									</div>
								</div>
								<div class="mt-2 flex items-center gap-1 border-t pt-2">
									<button
										title="Edit"
										class="flex h-7 w-7 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-muted hover:text-foreground"
										onclick={() => openEditDialog(item)}
									>
										<Pencil class="h-3.5 w-3.5" />
									</button>
									{#if item.status !== 'cancelled' && item.status !== 'completed'}
										<button
											title="Cancel"
											class="flex h-7 w-7 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-muted hover:text-foreground"
											onclick={() => handleMarkStatus(item.id, 'cancelled')}
										>
											<X class="h-3.5 w-3.5" />
										</button>
									{/if}
									<button
										title="Delete"
										class="ml-auto flex h-7 w-7 items-center justify-center rounded-md text-destructive transition-colors hover:bg-destructive/10"
										onclick={() => handleDeleteItem(item.id)}
									>
										<Trash2 class="h-3.5 w-3.5" />
									</button>
								</div>
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
			<Dialog.Title>New {newEventType === 'task' ? 'Task' : 'Event'}</Dialog.Title>
		</Dialog.Header>
		<div class="space-y-4 py-4">
			<div class="flex gap-2">
				<button
					type="button"
					class="flex-1 rounded-md border px-3 py-1.5 text-sm transition-colors {newEventType === 'event' ? 'border-primary bg-primary/10 text-primary' : 'border-input text-muted-foreground hover:bg-accent'}"
					onclick={() => { newEventType = 'event'; }}
				>
					Event
				</button>
				<button
					type="button"
					class="flex-1 rounded-md border px-3 py-1.5 text-sm transition-colors {newEventType === 'task' ? 'border-orange-500 bg-orange-500/10 text-orange-600' : 'border-input text-muted-foreground hover:bg-accent'}"
					onclick={() => { newEventType = 'task'; }}
				>
					Task
				</button>
			</div>
			<div>
				<Label>Title <span class="text-destructive">*</span></Label>
				<Input bind:value={newTitle} placeholder="{newEventType === 'task' ? 'Task name' : 'Event title'}" class="mt-1" />
			</div>
			<div class="grid grid-cols-2 gap-3">
				<div>
					<Label>{newEventType === 'task' ? 'Due date' : 'Start date'} <span class="text-destructive">*</span></Label>
					<input
						type="date"
						value={startDate}
						oninput={(e) => { startDate = e.currentTarget.value; }}
						class="mt-1 flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30"
					/>
				</div>
				<div>
					<Label>{newEventType === 'task' ? 'Due time' : 'Start time'} <span class="text-destructive">*</span></Label>
					<input
						type="text"
						inputmode="numeric"
						placeholder="HH:MM"
						maxlength={5}
						value={startTime}
						oninput={(e) => handleTimeInput(e, (v) => { startTime = v; })}
						class="mt-1 flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30"
					/>
				</div>
			</div>
			{#if newEventType === 'event'}
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
							type="text"
							inputmode="numeric"
							placeholder="HH:MM"
							maxlength={5}
							value={endTime}
							oninput={(e) => handleTimeInput(e, (v) => { endTime = v; })}
							class="mt-1 flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30"
						/>
					</div>
				</div>
			{/if}
			{#if newEventType === 'task'}
				<div>
					<Label>Priority</Label>
					<select
						class="mt-1 flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30"
						value={newPriority}
						onchange={(e) => { newPriority = e.currentTarget.value; }}
					>
						<option value="low">Low</option>
						<option value="medium">Medium</option>
						<option value="high">High</option>
						<option value="urgent">Urgent</option>
					</select>
				</div>
			{/if}
			{#if newEventType === 'event'}
				<div>
					<Label>Location</Label>
					<Input bind:value={newLocation} placeholder="Location" class="mt-1" />
				</div>
			{/if}
			<div>
				<Label>Description</Label>
				<Textarea
					bind:value={newDescription}
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

<Dialog.Root bind:open={showEditDialog}>
	<Dialog.Content class="sm:max-w-md">
		<Dialog.Header>
			<Dialog.Title>Edit {editingItem?.type === 'task' ? 'Task' : 'Event'}</Dialog.Title>
		</Dialog.Header>
		{#if editingItem}
			<div class="space-y-4 py-4">
				<div>
					<Label>Title <span class="text-destructive">*</span></Label>
					<Input bind:value={editTitle} class="mt-1" />
				</div>
				<div class="grid grid-cols-2 gap-3">
					<div>
						<Label>{editingItem.type === 'task' ? 'Due date' : 'Start date'} <span class="text-destructive">*</span></Label>
						<input
							type="date"
							value={editStartDate}
							oninput={(e) => { editStartDate = e.currentTarget.value; }}
							class="mt-1 flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30"
						/>
					</div>
					<div>
						<Label>{editingItem.type === 'task' ? 'Due time' : 'Start time'} <span class="text-destructive">*</span></Label>
						<input
							type="text"
							inputmode="numeric"
							placeholder="HH:MM"
							maxlength={5}
							value={editStartTime}
							oninput={(e) => handleTimeInput(e, (v) => { editStartTime = v; })}
							class="mt-1 flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30"
						/>
					</div>
				</div>
				{#if editingItem.type === 'event'}
					<div class="grid grid-cols-2 gap-3">
						<div>
							<Label>End date</Label>
							<input
								type="date"
								value={editEndDate}
								oninput={(e) => { editEndDate = e.currentTarget.value; }}
								class="mt-1 flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30"
							/>
						</div>
						<div>
							<Label>End time</Label>
							<input
								type="text"
								inputmode="numeric"
								placeholder="HH:MM"
								maxlength={5}
								value={editEndTime}
								oninput={(e) => handleTimeInput(e, (v) => { editEndTime = v; })}
								class="mt-1 flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30"
							/>
						</div>
					</div>
				{/if}
				{#if editingItem.type === 'task'}
					<div>
						<Label>Priority</Label>
						<select
							class="mt-1 flex h-9 w-full rounded-md border border-input bg-background px-3 py-1 text-base shadow-xs outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50 md:text-sm dark:bg-input/30"
							value={editPriority}
							onchange={(e) => { editPriority = e.currentTarget.value; }}
						>
							<option value="low">Low</option>
							<option value="medium">Medium</option>
							<option value="high">High</option>
							<option value="urgent">Urgent</option>
						</select>
					</div>
				{/if}
				{#if editingItem.type === 'event'}
					<div>
						<Label>Location</Label>
						<Input bind:value={editLocation} placeholder="Location" class="mt-1" />
					</div>
				{/if}
				<div>
					<Label>Description</Label>
					<Textarea
						bind:value={editDescription}
						placeholder="Description"
						class="mt-1 max-h-[120px] resize-none"
						rows={3}
					/>
				</div>
				{#if editError}
					<p class="text-sm text-destructive">{editError}</p>
				{/if}
			</div>
			<Dialog.Footer>
				<Button variant="outline" onclick={() => { showEditDialog = false; editingItem = null; }}>Cancel</Button>
				<Button onclick={handleSaveEdit} disabled={saving}>
					{saving ? 'Saving...' : 'Save'}
				</Button>
			</Dialog.Footer>
		{/if}
	</Dialog.Content>
</Dialog.Root>
