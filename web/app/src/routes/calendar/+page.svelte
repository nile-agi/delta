<script lang="ts">
	import { onMount } from 'svelte';
	import { ChevronLeft, ChevronRight, Plus, X } from '@lucide/svelte';
	import Button from '$lib/components/ui/button/button.svelte';
	import CalendarDayPanel from '$lib/components/app/calendar/CalendarDayPanel.svelte';
	import EventFormDialog from '$lib/components/app/calendar/EventFormDialog.svelte';
	import type { CalendarEvent } from '$lib/services/agent';
	import { config } from '$lib/stores/settings.svelte';
	import {
		calendarEvents,
		calendarCurrentMonth,
		setCurrentMonth,
		loadMonthEvents,
		createEvent,
		updateEvent,
		deleteEvent
	} from '$lib/stores/calendar.svelte';
	import {
		collectTags,
		formatTime,
		parseTags,
		sortDayItems,
		toLocalDateStr,
		weekdayColumn,
		weekdayNames
	} from '$lib/utils/calendar';

	let selectedDate = $state(toLocalDateStr(new Date()));
	let activeTag = $state('');
	let showForm = $state(false);
	let formMode = $state<'create' | 'edit'>('create');
	let editingItem = $state<CalendarEvent | null>(null);

	const month = $derived(calendarCurrentMonth());
	const events = $derived(calendarEvents());
	const weekStart = $derived(String(config().calendarWeekStart ?? 'monday'));
	const dayNames = $derived(weekdayNames(weekStart));
	const tags = $derived(collectTags(events));

	const monthLabel = $derived(
		month.toLocaleDateString('en-US', { month: 'long', year: 'numeric' })
	);

	const onCurrentMonth = $derived.by(() => {
		const now = new Date();
		return now.getFullYear() === month.getFullYear() && now.getMonth() === month.getMonth();
	});

	const visibleEvents = $derived(
		activeTag ? events.filter((e) => parseTags(e.tags ?? '').includes(activeTag)) : events
	);

	// One pass into a date -> items map; the grid would otherwise re-filter for every cell.
	const byDate = $derived.by(() => {
		const map = new Map<string, CalendarEvent[]>();
		for (const event of visibleEvents) {
			const key = (event.start_time ?? '').split('T')[0];
			if (!key) continue;
			const bucket = map.get(key);
			if (bucket) bucket.push(event);
			else map.set(key, [event]);
		}
		for (const [key, bucket] of map) map.set(key, sortDayItems(bucket));
		return map;
	});

	const selectedDayItems = $derived(byDate.get(selectedDate) ?? []);
	const todayStr = toLocalDateStr(new Date());

	// Full weeks, padded with the neighbouring months so the grid never has holes.
	const cells = $derived.by(() => {
		const year = month.getFullYear();
		const index = month.getMonth();
		const lead = weekdayColumn(new Date(year, index, 1).getDay(), weekStart);
		const length = new Date(year, index + 1, 0).getDate();
		const out: Array<{ date: string; day: number; inMonth: boolean }> = [];

		for (let back = lead; back > 0; back--) {
			const date = new Date(year, index, 1 - back);
			out.push({ date: toLocalDateStr(date), day: date.getDate(), inMonth: false });
		}
		for (let day = 1; day <= length; day++) {
			out.push({ date: toLocalDateStr(new Date(year, index, day)), day, inMonth: true });
		}
		for (let ahead = 1; out.length % 7 !== 0; ahead++) {
			const date = new Date(year, index + 1, ahead);
			out.push({ date: toLocalDateStr(date), day: date.getDate(), inMonth: false });
		}
		return out;
	});

	async function goToMonth(date: Date) {
		setCurrentMonth(date);
		await loadMonthEvents(date);
	}

	function shiftMonth(delta: number) {
		const next = new Date(month);
		next.setMonth(next.getMonth() + delta);
		goToMonth(next);
	}

	function goToToday() {
		selectedDate = todayStr;
		goToMonth(new Date());
	}

	// Clicking a padding day follows it into its own month rather than doing nothing.
	function selectCell(cell: { date: string; inMonth: boolean }) {
		selectedDate = cell.date;
		if (!cell.inMonth) goToMonth(new Date(cell.date + 'T00:00:00'));
	}

	function openCreate() {
		formMode = 'create';
		editingItem = null;
		showForm = true;
	}

	function openEdit(item: CalendarEvent) {
		formMode = 'edit';
		editingItem = item;
		showForm = true;
	}

	async function handleSubmit(data: Partial<CalendarEvent>) {
		if (formMode === 'edit' && editingItem) await updateEvent(editingItem.id, data);
		else await createEvent(data);
		await loadMonthEvents(month);
	}

	async function handleStatus(id: string, status: CalendarEvent['status']) {
		await updateEvent(id, { status });
		await loadMonthEvents(month);
	}

	async function handleDelete(id: string) {
		await deleteEvent(id);
		await loadMonthEvents(month);
	}

	function toggleTag(tag: string) {
		activeTag = activeTag === tag ? '' : tag;
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
	<div class="flex flex-wrap items-center gap-x-4 gap-y-2 border-b py-3 pl-14 pr-4">
		<div class="flex items-center gap-1">
			<Button variant="ghost" size="icon" class="size-8" onclick={() => shiftMonth(-1)}>
				<ChevronLeft class="size-4" />
			</Button>
			<Button variant="ghost" size="icon" class="size-8" onclick={() => shiftMonth(1)}>
				<ChevronRight class="size-4" />
			</Button>
			<Button
				variant="outline"
				size="sm"
				class="ml-1 h-8"
				disabled={onCurrentMonth && selectedDate === todayStr}
				onclick={goToToday}
			>
				Today
			</Button>
			<h1 class="ml-3 text-base font-semibold tracking-tight">{monthLabel}</h1>
		</div>

		{#if tags.length > 0}
			<div class="flex min-w-0 flex-1 flex-wrap items-center gap-1">
				{#each tags as tag (tag)}
					<button
						class="rounded-full border px-2 py-0.5 text-xs transition-colors {activeTag === tag
							? 'border-primary bg-primary/10 text-primary'
							: 'text-muted-foreground hover:border-foreground/30 hover:text-foreground'}"
						onclick={() => toggleTag(tag)}
					>
						{tag}
					</button>
				{/each}
				{#if activeTag}
					<button
						class="flex items-center gap-0.5 text-xs text-muted-foreground hover:text-foreground"
						onclick={() => (activeTag = '')}
					>
						<X class="size-3" /> clear
					</button>
				{/if}
			</div>
		{/if}

		<Button size="sm" class="ml-auto h-8" onclick={openCreate}>
			<Plus class="mr-1 size-4" />
			New
		</Button>
	</div>

	<div class="flex min-h-0 flex-1">
		<div class="flex min-w-0 flex-1 flex-col p-3">
			<div class="grid grid-cols-7">
				{#each dayNames as name (name)}
					<div class="pb-2 text-center text-[11px] font-medium uppercase tracking-wider text-muted-foreground">
						{name}
					</div>
				{/each}
			</div>

			<div
				class="grid flex-1 grid-cols-7 gap-px overflow-hidden rounded-lg border bg-border"
				style="grid-auto-rows: minmax(0, 1fr)"
			>
				{#each cells as cell (cell.date)}
					{@const items = byDate.get(cell.date) ?? []}
					{@const isToday = cell.date === todayStr}
					<button
						class="flex min-h-0 flex-col items-start overflow-hidden bg-background p-1.5 text-left transition-colors hover:bg-accent/50
							{cell.inMonth ? '' : 'bg-muted/30 text-muted-foreground'}
							{selectedDate === cell.date ? 'ring-2 ring-inset ring-primary' : ''}"
						onclick={() => selectCell(cell)}
					>
						<span
							class="mb-1 inline-flex size-6 shrink-0 items-center justify-center rounded-full text-xs tabular-nums
								{isToday ? 'bg-primary font-semibold text-primary-foreground' : ''}
								{!cell.inMonth && !isToday ? 'opacity-50' : ''}"
						>
							{cell.day}
						</span>

						<div class="flex min-h-0 w-full flex-col gap-px overflow-hidden">
							{#each items.slice(0, 3) as item (item.id)}
								<div
									class="flex w-full items-center gap-1 overflow-hidden rounded-sm border-l-2 pl-1 pr-0.5 text-[10px] leading-4
										{item.status === 'completed' || item.status === 'cancelled'
										? 'border-muted-foreground/40 text-muted-foreground line-through'
										: item.type === 'task'
											? 'border-orange-500 text-foreground'
											: 'border-primary text-foreground'}"
								>
									{#if !item.all_day}
										<span class="shrink-0 tabular-nums text-muted-foreground">
											{formatTime(item.start_time)}
										</span>
									{/if}
									<span class="truncate">{item.title}</span>
								</div>
							{/each}
							{#if items.length > 3}
								<span class="pl-1 text-[10px] text-muted-foreground">
									+{items.length - 3} more
								</span>
							{/if}
						</div>
					</button>
				{/each}
			</div>
		</div>

		<CalendarDayPanel
			date={selectedDate}
			items={selectedDayItems}
			onEdit={openEdit}
			onStatus={handleStatus}
			onDelete={handleDelete}
			onTagSelect={toggleTag}
		/>
	</div>
</div>

<EventFormDialog
	bind:open={showForm}
	mode={formMode}
	item={editingItem}
	defaultDate={selectedDate}
	onSubmit={handleSubmit}
/>
