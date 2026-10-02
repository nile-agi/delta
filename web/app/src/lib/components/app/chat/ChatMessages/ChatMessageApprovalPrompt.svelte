<script lang="ts">
	import { ShieldAlert } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import { Card } from '$lib/components/ui/card';
	import { agentStore } from '$lib/stores/agent.svelte';

	interface Props {
		class?: string;
		messageId: string;
	}

	let { class: className = '', messageId }: Props = $props();

	// Only render on the message the request actually came from.
	const pending = $derived(
		agentStore.pendingApproval?.messageId === messageId ? agentStore.pendingApproval.request : null
	);

	function formatArguments(args: Record<string, unknown>): string {
		const entries = Object.entries(args ?? {});
		if (entries.length === 0) return '';
		return entries
			.map(([key, value]) => `${key}: ${typeof value === 'string' ? value : JSON.stringify(value)}`)
			.join('\n');
	}

	async function respond(decision: AgentApprovalDecision) {
		await agentStore.respond(decision);
	}
</script>

{#if pending}
	<Card class="mb-6 gap-0 border-amber-500/40 bg-amber-500/5 p-3 {className}">
		<div class="flex items-start gap-2">
			<ShieldAlert class="mt-0.5 h-4 w-4 shrink-0 text-amber-500" />

			<div class="min-w-0 flex-1">
				<p class="text-sm font-medium">
					Delta wants to run <span class="font-mono">{pending.name}</span>
				</p>

				{#if pending.description}
					<p class="mt-1 text-xs text-muted-foreground">{pending.description}</p>
				{/if}

				{#if formatArguments(pending.arguments)}
					<pre
						class="mt-2 max-h-40 overflow-auto rounded bg-muted/60 p-2 text-xs whitespace-pre-wrap">{formatArguments(
							pending.arguments
						)}</pre>
				{/if}

				<div class="mt-3 flex flex-wrap gap-2">
					<Button
						size="sm"
						disabled={agentStore.isRespondingToApproval}
						onclick={() => respond('allow')}
					>
						Allow once
					</Button>

					<Button
						size="sm"
						variant="secondary"
						disabled={agentStore.isRespondingToApproval}
						onclick={() => respond('always')}
					>
						Always allow this tool
					</Button>

					<Button
						size="sm"
						variant="outline"
						disabled={agentStore.isRespondingToApproval}
						onclick={() => respond('deny')}
					>
						Not this time
					</Button>

					<Button
						size="sm"
						variant="ghost"
						disabled={agentStore.isRespondingToApproval}
						onclick={() => respond('never')}
					>
						Never allow
					</Button>
				</div>

				<p class="mt-2 text-[11px] text-muted-foreground">
					“Always” and “Never” are remembered for this tool across conversations. Settings →
					Agent tools → “Forget all” resets them.
				</p>
			</div>
		</div>
	</Card>
{/if}
