<script lang="ts">
	import {
		AlertTriangle,
		Ban,
		Check,
		ChevronsUpDownIcon,
		Loader2,
		Scissors,
		Terminal,
		X
	} from '@lucide/svelte';
	import * as Collapsible from '$lib/components/ui/collapsible/index.js';
	import { buttonVariants } from '$lib/components/ui/button/index.js';
	import { Card } from '$lib/components/ui/card';

	interface Props {
		class?: string;
		activity: AgentActivity | undefined;
	}

	let { class: className = '', activity }: Props = $props();

	const steps = $derived<AgentActivityStep[]>(activity?.steps ?? []);
	const notices = $derived<string[]>(activity?.notices ?? []);
	const compaction = $derived(activity?.compaction);

	const isWorking = $derived(
		steps.some((step) => step.status === 'running' || step.status === 'awaiting-approval')
	);
	const failureCount = $derived(
		steps.filter((step) => step.status === 'failed' || step.status === 'denied').length
	);

	const headline = $derived.by(() => {
		if (isWorking) {
			const active = steps.find(
				(step) => step.status === 'running' || step.status === 'awaiting-approval'
			);
			return active?.status === 'awaiting-approval'
				? `Waiting for your approval: ${active.name}`
				: `Using ${active?.name ?? 'a tool'}...`;
		}
		if (steps.length === 0) return 'Agent activity';
		const noun = steps.length === 1 ? 'step' : 'steps';
		return failureCount > 0
			? `${steps.length} ${noun}, ${failureCount} did not complete`
			: `${steps.length} ${noun}`;
	});

	// Open while the run is live so the user can watch it work, then collapse once it settles.
	let manuallyToggled = $state(false);
	let isExpanded = $state(false);

	$effect(() => {
		if (!manuallyToggled) {
			isExpanded = isWorking;
		}
	});

	function onOpenChange(open: boolean) {
		manuallyToggled = true;
		isExpanded = open;
	}

	/** Renders tool arguments as a single readable line rather than raw JSON. */
	function describeArguments(args: Record<string, unknown>): string {
		const entries = Object.entries(args ?? {});
		if (entries.length === 0) return '';
		return entries
			.map(([key, value]) => {
				let text = typeof value === 'string' ? value : JSON.stringify(value);
				if (text && text.length > 120) text = `${text.slice(0, 120)}…`;
				return `${key}: ${text}`;
			})
			.join('  ·  ');
	}

	function durationLabel(step: AgentActivityStep): string {
		if (!step.endedAt) return '';
		const ms = step.endedAt - step.startedAt;
		return ms < 1000 ? `${ms}ms` : `${(ms / 1000).toFixed(1)}s`;
	}
</script>

{#if steps.length > 0 || notices.length > 0 || compaction}
	<Collapsible.Root open={isExpanded} {onOpenChange} class="mb-6 {className}">
		<Card class="gap-0 border-muted bg-muted/30 py-0">
			<Collapsible.Trigger class="flex w-full cursor-pointer items-center justify-between p-3">
				<div class="flex items-center gap-2 text-muted-foreground">
					{#if isWorking}
						<Loader2 class="h-4 w-4 animate-spin" />
					{:else if failureCount > 0}
						<AlertTriangle class="h-4 w-4" />
					{:else}
						<Terminal class="h-4 w-4" />
					{/if}

					<span class="text-sm font-medium">{headline}</span>
				</div>

				<div
					class={buttonVariants({
						variant: 'ghost',
						size: 'sm',
						class: 'h-6 w-6 p-0 text-muted-foreground hover:text-foreground'
					})}
				>
					<ChevronsUpDownIcon class="h-4 w-4" />
					<span class="sr-only">Toggle agent activity</span>
				</div>
			</Collapsible.Trigger>

			<Collapsible.Content>
				<div class="space-y-2 border-t border-muted px-3 pt-3 pb-3">
					{#each steps as step (step.id)}
						<div class="flex items-start gap-2 text-xs">
							<span class="mt-0.5 shrink-0">
								{#if step.status === 'running'}
									<Loader2 class="h-3.5 w-3.5 animate-spin text-muted-foreground" />
								{:else if step.status === 'awaiting-approval'}
									<AlertTriangle class="h-3.5 w-3.5 text-amber-500" />
								{:else if step.status === 'success'}
									<Check class="h-3.5 w-3.5 text-emerald-500" />
								{:else if step.status === 'denied'}
									<Ban class="h-3.5 w-3.5 text-muted-foreground" />
								{:else}
									<X class="h-3.5 w-3.5 text-destructive" />
								{/if}
							</span>

							<div class="min-w-0 flex-1">
								<div class="flex items-baseline gap-2">
									<span class="font-medium">{step.name}</span>
									{#if step.risk === 'destructive'}
										<span class="rounded bg-destructive/10 px-1 text-[10px] text-destructive">
											destructive
										</span>
									{/if}
									{#if durationLabel(step)}
										<span class="text-[10px] text-muted-foreground">{durationLabel(step)}</span>
									{/if}
								</div>

								{#if describeArguments(step.arguments)}
									<div class="mt-0.5 break-words text-muted-foreground">
										{describeArguments(step.arguments)}
									</div>
								{/if}

								{#if step.error}
									<div class="mt-0.5 break-words text-destructive">{step.error}</div>
								{:else if step.summary}
									<div class="mt-0.5 break-words text-muted-foreground">{step.summary}</div>
								{/if}
							</div>
						</div>
					{/each}

					{#if compaction && compaction.dropped > 0}
						<div class="flex items-start gap-2 pt-1 text-xs text-muted-foreground">
							<Scissors class="mt-0.5 h-3.5 w-3.5 shrink-0" />
							<span>
								{compaction.summarized
									? `Summarized ${compaction.dropped} earlier messages to stay inside the context window`
									: `Dropped ${compaction.dropped} earlier messages to stay inside the context window`}
								({compaction.used_tokens} / {compaction.budget_tokens} tokens)
							</span>
						</div>
					{/if}

					{#each notices as notice, index (index)}
						<div class="pt-1 text-xs text-muted-foreground">{notice}</div>
					{/each}
				</div>
			</Collapsible.Content>
		</Card>
	</Collapsible.Root>
{/if}
