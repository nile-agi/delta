<script lang="ts">
	import { Check, X, Undo2, Trash2, MapPin, Play, Pencil } from '@lucide/svelte';
	import type { CalendarEvent } from '$lib/services/agent';
	import { formatTime, parseTags, relativeDayLabel, sortDayItems } from '$lib/utils/calendar';

	interface Props {
		date: string;
		items: CalendarEvent[];
		onEdit: (item: CalendarEvent) => void;
		onStatus: (id: string, status: CalendarEvent['status']) => void;
		onDelete: (id: string) => void;
		onTagSelect: (tag: string) => void;
	}

	let { date, items, onEdit, onStatus, onDelete, onTagSelect }: Props = $props();

	const sorted = $derived(sortDayItems(items));
	const relative = $derived(relativeDayLabel(date));

	const heading = $derived(
		new Date(date + 'T00:00:00').toLocaleDateString('en-US', {
			weekday: 'long',
			month: 'short',
			day: 'numeric'
		})
	);

	function isDone(item: CalendarEvent) {
		return item.status === 'completed' || item.status === 'cancelled';
	}
</script>

<div class="flex w-80 flex-col overflow-hidden border-l">
	<div class="flex items-baseline gap-2 border-b px-4 py-3">
		<h2 class="text-sm font-semibold">{heading}</h2>
		{#if relative}
			<span class="rounded-full bg-muted px-1.5 py-0.5 text-[10px] text-muted-foreground">
				{relative}
			</span>
		{/if}
	</div>

	<div class="flex-1 overflow-y-auto px-4 py-3">
		{#if sorted.length === 0}
			<p class="pt-6 text-center text-sm text-muted-foreground">Nothing scheduled.</p>
		{:else}
			<!-- Time rail: a single hairline with a node per item, so the day reads as a shape. -->
			<div class="relative">
				<span class="absolute bottom-2 left-14 top-2 w-px bg-border"></span>

				{#each sorted as item (item.id)}
					{@const done = isDone(item)}
					<div class="relative flex pb-4 last:pb-0">
						<div
							class="w-14 shrink-0 pr-3 pt-0.5 text-right text-[11px] tabular-nums text-muted-foreground"
							class:opacity-50={done}
						>
							{#if item.all_day}
								All day
							{:else}
								{formatTime(item.start_time)}
							{/if}
						</div>

						<span
							class="absolute left-14 mt-[7px] size-2 -translate-x-1/2 rounded-full ring-4 ring-background
								{item.status === 'completed'
								? 'bg-green-500'
								: item.status === 'cancelled'
									? 'bg-muted-foreground/40'
									: item.status === 'in_progress'
										? 'bg-blue-500'
										: item.type === 'task'
											? 'bg-orange-500'
											: 'bg-primary'}"
						></span>

						<div class="min-w-0 flex-1 pl-4" class:opacity-50={done}>
							<div class="flex items-start gap-2">
								<p class="min-w-0 flex-1 text-sm font-medium" class:line-through={done}>
									{item.title}
								</p>

								{#if item.status === 'completed'}
									<button
										class="mt-0.5 flex size-5 shrink-0 items-center justify-center rounded-full bg-green-500 text-white"
										title="Move back to upcoming"
										onclick={() => onStatus(item.id, 'upcoming')}
									>
										<Check class="size-3" />
									</button>
								{:else if item.status === 'cancelled'}
									<button
										class="mt-0.5 flex size-5 shrink-0 items-center justify-center rounded-full bg-muted text-muted-foreground"
										title="Restore"
										onclick={() => onStatus(item.id, 'upcoming')}
									>
										<Undo2 class="size-3" />
									</button>
								{:else if item.status === 'in_progress'}
									<button
										class="mt-0.5 flex size-5 shrink-0 items-center justify-center rounded-full bg-blue-500 text-white"
										title="Mark as done"
										onclick={() => onStatus(item.id, 'completed')}
									>
										<Play class="size-2.5" />
									</button>
								{:else}
									<button
										class="mt-0.5 size-5 shrink-0 rounded-full border-2 border-muted-foreground/30 transition-colors hover:border-green-500 hover:bg-green-500/10"
										title="Mark as done"
										aria-label="Mark as done"
										onclick={() => onStatus(item.id, 'completed')}
									></button>
								{/if}
							</div>

							{#if !item.all_day && item.end_time}
								<p class="text-[11px] tabular-nums text-muted-foreground">
									until {formatTime(item.end_time)}
								</p>
							{/if}

							{#if item.location}
								<p class="mt-1 flex items-center gap-1 text-xs text-muted-foreground">
									<MapPin class="size-3 shrink-0" />
									<span class="truncate">{item.location}</span>
								</p>
							{/if}

							<div class="mt-1 flex flex-wrap items-center gap-1">
								{#if item.type === 'task' && item.priority && item.priority !== 'medium'}
									<span
										class="rounded px-1 text-[10px] font-medium {item.priority === 'urgent'
											? 'bg-red-500/10 text-red-600 dark:text-red-400'
											: item.priority === 'high'
												? 'bg-orange-500/10 text-orange-600 dark:text-orange-400'
												: 'bg-muted text-muted-foreground'}"
									>
										{item.priority}
									</span>
								{/if}
								{#if item.status === 'in_progress'}
									<span
										class="rounded bg-blue-500/10 px-1 text-[10px] font-medium text-blue-600 dark:text-blue-400"
									>
										in progress
									</span>
								{/if}
								{#each parseTags(item.tags ?? '') as tag (tag)}
									<button
										class="rounded-full border px-1.5 text-[10px] text-muted-foreground transition-colors hover:border-primary hover:text-foreground"
										onclick={() => onTagSelect(tag)}
									>
										{tag}
									</button>
								{/each}
							</div>

							{#if item.description}
								<p class="mt-1.5 rounded bg-muted/50 px-2 py-1 text-xs text-muted-foreground">
									{item.description}
								</p>
							{/if}

							<div class="mt-1.5 flex items-center gap-0.5">
								<button
									title="Edit"
									class="flex size-7 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-muted hover:text-foreground"
									onclick={() => onEdit(item)}
								>
									<Pencil class="size-3.5" />
								</button>
								{#if !done}
									<button
										title="Cancel"
										class="flex size-7 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-muted hover:text-foreground"
										onclick={() => onStatus(item.id, 'cancelled')}
									>
										<X class="size-3.5" />
									</button>
								{/if}
								<button
									title="Delete"
									class="ml-auto flex size-7 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-destructive/10 hover:text-destructive"
									onclick={() => onDelete(item.id)}
								>
									<Trash2 class="size-3.5" />
								</button>
							</div>
						</div>
					</div>
				{/each}
			</div>
		{/if}
	</div>
</div>
