<script lang="ts">
	import { Clock, Calendar, CheckCircle, X, ChevronDown, ChevronUp } from '@lucide/svelte';
	import { fly, fade } from 'svelte/transition';
	import {
		activeNotifications,
		dismissNotification,
		dismissAll
	} from '$lib/stores/notifications.svelte';
	import { updateEvent } from '$lib/stores/calendar.svelte';
	import type { CalendarEvent } from '$lib/services/agent';
	import { goto } from '$app/navigation';

	let expanded = $state<Record<string, boolean>>({});
	const items = $derived(activeNotifications());
	let prevCount = $state(0);

	function playChime() {
		try {
			const ctx = new AudioContext();
			const now = ctx.currentTime;

			const g = ctx.createGain();
			g.gain.setValueAtTime(0.18, now);
			g.gain.exponentialRampToValueAtTime(0.001, now + 0.45);
			g.connect(ctx.destination);

			const o1 = ctx.createOscillator();
			o1.type = 'sine';
			o1.frequency.setValueAtTime(880, now);
			o1.connect(g);
			o1.start(now);
			o1.stop(now + 0.15);

			const g2 = ctx.createGain();
			g2.gain.setValueAtTime(0.14, now + 0.12);
			g2.gain.exponentialRampToValueAtTime(0.001, now + 0.55);
			g2.connect(ctx.destination);

			const o2 = ctx.createOscillator();
			o2.type = 'sine';
			o2.frequency.setValueAtTime(1175, now + 0.12);
			o2.connect(g2);
			o2.start(now + 0.12);
			o2.stop(now + 0.35);

			setTimeout(() => ctx.close(), 700);
		} catch {
			// Audio not available
		}
	}

	$effect(() => {
		if (items.length > prevCount) {
			playChime();
		}
		prevCount = items.length;
	});

	function toggle(id: string) {
		expanded = { ...expanded, [id]: !expanded[id] };
	}

	function formatTime(time: string | undefined): string {
		if (!time) return '';
		try {
			return new Date(time).toLocaleTimeString('en-US', {
				hour: 'numeric',
				minute: '2-digit'
			});
		} catch {
			return time;
		}
	}

	async function markDone(n: { eventId?: string; id: string }) {
		if (n.eventId) {
			await updateEvent(n.eventId, { status: 'completed' } as Partial<CalendarEvent>);
		}
		dismissNotification(n.id);
	}

	function viewInCalendar(n: { time?: string; id: string }) {
		dismissNotification(n.id);
		goto('#/calendar');
	}
</script>

{#if items.length > 0}
	<div class="fixed left-1/2 top-4 z-[100000] flex w-full max-w-sm -translate-x-1/2 flex-col gap-2">
		{#each items as item (item.id)}
			<div
				class="group relative overflow-hidden rounded-xl border border-border/60 bg-card shadow-lg shadow-black/20 backdrop-blur-sm"
				in:fly={{ y: -40, duration: 250 }}
				out:fade={{ duration: 150 }}
			>
				<!-- Accent stripe -->
				<div
					class="absolute left-0 top-0 h-full w-1 {item.type === 'task'
						? 'bg-orange-500'
						: 'bg-primary'}"
				></div>

				<!-- Compact row -->
				<button
					class="flex w-full items-center gap-3 px-4 py-3 text-left"
					onclick={() => toggle(item.id)}
				>
					<div
						class="flex h-8 w-8 shrink-0 items-center justify-center rounded-full {item.type === 'task'
							? 'bg-orange-500/15 text-orange-500'
							: 'bg-primary/15 text-primary'}"
					>
						{#if item.type === 'task'}
							<CheckCircle class="h-4 w-4" />
						{:else}
							<Calendar class="h-4 w-4" />
						{/if}
					</div>
					<div class="min-w-0 flex-1">
						<p class="truncate text-sm font-medium text-card-foreground">{item.title}</p>
						<p class="text-xs text-muted-foreground">{item.body}</p>
					</div>
					<div class="flex items-center gap-1">
						{#if expanded[item.id]}
							<ChevronUp class="h-4 w-4 text-muted-foreground" />
						{:else}
							<ChevronDown class="h-4 w-4 text-muted-foreground" />
						{/if}
					</div>
				</button>

				<!-- Dismiss button (always visible on hover) -->
				<button
					class="absolute right-2 top-2 flex h-5 w-5 items-center justify-center rounded-full text-muted-foreground opacity-0 transition-opacity hover:bg-muted hover:text-foreground group-hover:opacity-100"
					onclick={() => dismissNotification(item.id)}
				>
					<X class="h-3 w-3" />
				</button>

				<!-- Expanded details -->
				{#if expanded[item.id]}
					<div
						class="border-t border-border/40 px-4 py-2.5"
						in:fly={{ y: -8, duration: 150 }}
					>
						{#if item.time}
							<div class="flex items-center gap-1.5 text-xs text-muted-foreground">
								<Clock class="h-3 w-3" />
								{formatTime(item.time)}
							</div>
						{/if}
						<div class="mt-2 flex items-center gap-2">
							<button
								class="rounded-md bg-primary px-3 py-1 text-xs font-medium text-primary-foreground transition-colors hover:bg-primary/80"
								onclick={() => viewInCalendar(item)}
							>
								View
							</button>
							{#if item.type === 'task' && item.eventId}
								<button
									class="rounded-md bg-muted px-3 py-1 text-xs font-medium text-foreground transition-colors hover:bg-muted/80"
									onclick={() => markDone(item)}
								>
									Mark done
								</button>
							{/if}
							<button
								class="rounded-md px-3 py-1 text-xs text-muted-foreground transition-colors hover:bg-muted hover:text-foreground"
								onclick={() => dismissNotification(item.id)}
							>
								Dismiss
							</button>
						</div>
					</div>
				{/if}
			</div>
		{/each}

		{#if items.length > 1}
			<button
				class="self-center rounded-md px-3 py-1 text-xs text-muted-foreground transition-colors hover:bg-muted hover:text-foreground"
				onclick={dismissAll}
			>
				Dismiss all
			</button>
		{/if}
	</div>
{/if}
