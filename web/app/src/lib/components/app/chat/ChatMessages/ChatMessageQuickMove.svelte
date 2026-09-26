<script lang="ts">
	import { ArrowRight, CalendarCheck, CalendarClock, X } from '@lucide/svelte';
	import { agentService } from '$lib/services/agent';
	import { openCalendarWindow } from '$lib/services/calendar-window';
	import { setQuickAdd } from '$lib/stores/chat.svelte';
	import type { QuickMoveOption, QuickMoveSuggestion } from '$lib/types/agent';
	import { describeWhen } from '$lib/utils/calendar';
	import { toast } from 'svelte-sonner';

	interface Props {
		messageId: string;
		suggestion: QuickMoveSuggestion;
	}

	let { messageId, suggestion }: Props = $props();

	let saving = $state(false);

	async function move(option: QuickMoveOption) {
		saving = true;
		try {
			await agentService.updateEvent(option.id, {
				start_time: option.new_start_time,
				...(option.new_end_time ? { end_time: option.new_end_time } : {})
			});
			await setQuickAdd(messageId, { ...suggestion, status: 'moved', moved: option });
		} catch (error) {
			toast.error(error instanceof Error ? error.message : 'Could not move it');
		} finally {
			saving = false;
		}
	}

	function dismiss() {
		void setQuickAdd(messageId, { ...suggestion, status: 'dismissed' });
	}
</script>

{#if suggestion.status === 'offered' && suggestion.options.length > 0}
	<div class="flex max-w-[80%] flex-col items-end gap-1">
		{#if suggestion.options.length > 1}
			<span class="px-1 text-xs text-muted-foreground">Which one should move?</span>
		{/if}
		{#each suggestion.options as option (option.id)}
			<div
				class="flex max-w-full items-center gap-1 rounded-full border border-border/70 bg-muted/40 p-0.5 pl-1 text-xs text-muted-foreground"
			>
				<button
					class="flex min-w-0 items-center gap-1.5 rounded-full px-2 py-1 text-left transition-colors hover:bg-accent hover:text-accent-foreground disabled:opacity-60"
					disabled={saving}
					onclick={() => move(option)}
					title="Move it in your calendar"
				>
					<CalendarClock class="h-3.5 w-3.5 shrink-0" />
					<span class="shrink-0 font-medium text-foreground">Move</span>
					<span class="truncate">· {option.title} · {describeWhen(option.start_time, option.all_day)}</span>
					<ArrowRight class="h-3 w-3 shrink-0" />
					<span class="shrink-0 text-foreground">{describeWhen(option.new_start_time, option.all_day)}</span>
				</button>
				{#if suggestion.options.length === 1}
					<button
						class="rounded-full p-1.5 transition-colors hover:bg-accent hover:text-accent-foreground"
						aria-label="Dismiss suggestion"
						title="Dismiss"
						onclick={dismiss}
					>
						<X class="h-3 w-3" />
					</button>
				{/if}
			</div>
		{/each}
		{#if suggestion.options.length > 1}
			<button class="px-1 text-xs text-muted-foreground hover:text-foreground" onclick={dismiss}>
				None of these
			</button>
		{/if}
	</div>
{:else if suggestion.status === 'moved' && suggestion.moved}
	<div class="flex max-w-[80%] items-center gap-1.5 px-1 text-xs text-muted-foreground">
		<CalendarCheck class="h-3.5 w-3.5 shrink-0 text-emerald-500" />
		<span class="truncate">
			Moved · {suggestion.moved.title} · {describeWhen(suggestion.moved.new_start_time, suggestion.moved.all_day)}
		</span>
		<button class="shrink-0 underline-offset-2 hover:text-foreground hover:underline" onclick={openCalendarWindow}>
			Open
		</button>
	</div>
{/if}
