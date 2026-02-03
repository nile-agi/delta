<script lang="ts">
	import { BookOpen, Sparkles, Copy, Gauge, Clock, WholeWord } from '@lucide/svelte';
	import { copyToClipboard } from '$lib/utils/copy';

	interface Props {
		promptTokens: number;
		promptMs: number;
		predictedTokens: number;
		predictedMs: number;
	}

	let { promptTokens, promptMs, predictedTokens, predictedMs }: Props = $props();

	let statsView = $state<'reading' | 'generation'>('generation');

	const promptSpeed = $derived(promptMs > 0 ? (promptTokens / promptMs) * 1000 : 0);
	const genSpeed = $derived(predictedMs > 0 ? (predictedTokens / predictedMs) * 1000 : 0);

	async function copyStat(value: string, label: string) {
		await copyToClipboard(value, `${label} copied`);
	}
</script>

<div class="flex flex-wrap items-center gap-3">
	<div class="flex items-center gap-1 rounded-md border border-border/50 bg-muted/20 p-0.5">
		<button
			type="button"
			class="inline-flex items-center justify-center rounded px-2 py-1.5 transition {statsView ===
			'reading'
				? 'bg-background text-foreground shadow-sm'
				: 'text-muted-foreground hover:text-foreground'}"
			title="Reading (prompt processing)"
			aria-label="Reading (prompt processing)"
			onclick={() => (statsView = 'reading')}
		>
			<BookOpen class="h-3.5 w-3.5 shrink-0" />
		</button>
		<button
			type="button"
			class="inline-flex items-center justify-center rounded px-2 py-1.5 transition {statsView ===
			'generation'
				? 'bg-background text-foreground shadow-sm'
				: 'text-muted-foreground hover:text-foreground'}"
			title="Generation (token output)"
			aria-label="Generation (token output)"
			onclick={() => (statsView = 'generation')}
		>
			<Sparkles class="h-3.5 w-3.5 shrink-0" />
		</button>
	</div>

	{#if statsView === 'reading'}
		<div class="inline-flex flex-wrap items-center gap-2">
			<button
				type="button"
				class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
				title="Copy prompt tokens"
				onclick={() => copyStat(String(promptTokens), 'Prompt tokens')}
			>
				<WholeWord class="h-3 w-3" />
				<span>{promptTokens} tokens</span>
				<Copy class="h-3 w-3 opacity-70" />
			</button>
			<button
				type="button"
				class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
				title="Copy prompt processing time"
				onclick={() => copyStat(`${(promptMs / 1000).toFixed(2)}s`, 'Prompt processing time')}
			>
				<Clock class="h-3 w-3" />
				<span>{(promptMs / 1000).toFixed(2)}s</span>
				<Copy class="h-3 w-3 opacity-70" />
			</button>
			<button
				type="button"
				class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
				title="Copy prompt processing speed"
				onclick={() => copyStat(`${promptSpeed.toFixed(2)} tokens/s`, 'Prompt processing speed')}
			>
				<Gauge class="h-3 w-3" />
				<span>{promptSpeed.toFixed(2)} tokens/s</span>
				<Copy class="h-3 w-3 opacity-70" />
			</button>
		</div>
	{:else}
		<div class="inline-flex flex-wrap items-center gap-2">
			<button
				type="button"
				class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
				title="Generated tokens"
				onclick={() => copyStat(String(predictedTokens), 'Generated tokens')}
			>
				<WholeWord class="h-3 w-3" />
				<span>{predictedTokens} tokens</span>
				<Copy class="h-3 w-3 opacity-70" />
			</button>
			<button
				type="button"
				class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
				title="Generation time"
				onclick={() => copyStat(`${(predictedMs / 1000).toFixed(2)}s`, 'Generation time')}
			>
				<Clock class="h-3 w-3" />
				<span>{(predictedMs / 1000).toFixed(2)}s</span>
				<Copy class="h-3 w-3 opacity-70" />
			</button>
			<button
				type="button"
				class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
				title="Generation speed"
				onclick={() => copyStat(`${genSpeed.toFixed(2)} tokens/s`, 'Generation speed')}
			>
				<Gauge class="h-3 w-3" />
				<span>{genSpeed.toFixed(2)} tokens/s</span>
				<Copy class="h-3 w-3 opacity-70" />
			</button>
		</div>
	{/if}
</div>
