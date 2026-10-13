<script lang="ts">
	import { FileText } from '@lucide/svelte';
	import { citationLabel, sourceToTarget, type CitationTarget } from '$lib/utils/citations';

	interface Props {
		sources: AgentSource[];
		onOpen: (target: CitationTarget) => void;
	}

	let { sources, onOpen }: Props = $props();
</script>

{#if sources.length > 0}
	<div class="mt-3 flex flex-wrap items-center gap-2 text-xs" aria-label="Sources">
		<span class="text-muted-foreground">Sources</span>
		{#each sources as source, i (i)}
			{@const target = sourceToTarget(source)}
			<button
				type="button"
				class="inline-flex max-w-full items-center gap-1.5 rounded-md border border-border/60 bg-muted/40 px-2 py-1 hover:bg-accent focus-visible:ring-2 focus-visible:ring-ring focus-visible:outline-none"
				title="Open {source.document} at {citationLabel(target)}"
				onclick={() => onOpen(target)}
			>
				<FileText class="h-3.5 w-3.5 shrink-0" />
				<span class="truncate">{source.document}</span>
				{#if citationLabel(target)}<span class="text-muted-foreground">· {citationLabel(target)}</span>{/if}
			</button>
		{/each}
	</div>
{/if}
