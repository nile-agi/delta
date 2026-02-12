<script lang="ts">
	import { BookOpen, Sparkles, Copy, Gauge, Clock, WholeWord } from '@lucide/svelte';
	import { copyToClipboard } from '$lib/utils/copy';

	interface Props {
		promptTokens: number;
		promptMs: number;
		predictedTokens: number;
		predictedMs: number;
		/** When set and not idle, stats are shown in real time from this state */
		liveState?: ApiProcessingState | null;
	}

	let { promptTokens, promptMs, predictedTokens, predictedMs, liveState = null }: Props = $props();

	let statsView = $state<'reading' | 'generation'>('generation');

	// Use live state when active; otherwise use final timings from props
	const isLive = $derived(liveState != null && liveState.status !== 'idle');

	const effectivePromptTokens = $derived(
		isLive && liveState?.status === 'preparing'
			? (liveState.promptProgressProcessed ?? liveState.promptTokens ?? 0)
			: promptTokens
	);
	const effectivePromptMs = $derived(
		isLive && liveState?.status === 'preparing' && liveState.promptProgressTimeMs != null
			? liveState.promptProgressTimeMs
			: promptMs
	);
	const effectivePredictedTokens = $derived(
		isLive && liveState?.status === 'generating' ? liveState.tokensDecoded : predictedTokens
	);
	const effectivePredictedMs = $derived(
		isLive && liveState?.status === 'generating'
			? liveState.generationTimeMs ??
					(liveState.tokensPerSecond && liveState.tokensPerSecond > 0
						? (liveState.tokensDecoded / liveState.tokensPerSecond) * 1000
						: 0)
			: predictedMs
	);

	const promptSpeed = $derived(
		effectivePromptMs > 0
			? (effectivePromptTokens / effectivePromptMs) * 1000
			: (liveState?.promptTokensPerSecond ?? 0)
	);
	const genSpeed = $derived(
		effectivePredictedMs > 0
			? (effectivePredictedTokens / effectivePredictedMs) * 1000
			: (liveState?.tokensPerSecond ?? 0)
	);

	// When live, sync tab to current phase
	$effect(() => {
		if (!liveState || liveState.status === 'idle') return;
		if (liveState.status === 'preparing') statsView = 'reading';
		else if (liveState.status === 'generating') statsView = 'generation';
	});

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
				aria-label="Prompt tokens: {effectivePromptTokens}. Copy to clipboard"
				onclick={() => copyStat(String(effectivePromptTokens), 'Prompt tokens')}
			>
				<WholeWord class="h-3 w-3" />
				<span>{effectivePromptTokens} tokens</span>
				<Copy class="h-3 w-3 opacity-70" />
			</button>
			<button
				type="button"
				class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
				title="Copy prompt processing time"
				aria-label="Prompt processing time: {(effectivePromptMs / 1000).toFixed(2)} seconds. Copy to clipboard"
				onclick={() =>
					copyStat(`${(effectivePromptMs / 1000).toFixed(2)}s`, 'Prompt processing time')}
			>
				<Clock class="h-3 w-3" />
				<span>{(effectivePromptMs / 1000).toFixed(2)}s</span>
				<Copy class="h-3 w-3 opacity-70" />
			</button>
			<button
				type="button"
				class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
				title="Copy prompt processing speed"
				aria-label="Prompt processing speed: {promptSpeed.toFixed(2)} tokens per second. Copy to clipboard"
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
				aria-label="Generated tokens: {effectivePredictedTokens}. Copy to clipboard"
				onclick={() => copyStat(String(effectivePredictedTokens), 'Generated tokens')}
			>
				<WholeWord class="h-3 w-3" />
				<span>{effectivePredictedTokens} tokens</span>
				<Copy class="h-3 w-3 opacity-70" />
			</button>
			<button
				type="button"
				class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
				title="Generation time"
				aria-label="Generation time: {(effectivePredictedMs / 1000).toFixed(2)} seconds. Copy to clipboard"
				onclick={() =>
					copyStat(`${(effectivePredictedMs / 1000).toFixed(2)}s`, 'Generation time')}
			>
				<Clock class="h-3 w-3" />
				<span>{(effectivePredictedMs / 1000).toFixed(2)}s</span>
				<Copy class="h-3 w-3 opacity-70" />
			</button>
			<button
				type="button"
				class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
				title="Generation speed"
				aria-label="Generation speed: {genSpeed.toFixed(2)} tokens per second. Copy to clipboard"
				onclick={() => copyStat(`${genSpeed.toFixed(2)} tokens/s`, 'Generation speed')}
			>
				<Gauge class="h-3 w-3" />
				<span>{genSpeed.toFixed(2)} tokens/s</span>
				<Copy class="h-3 w-3 opacity-70" />
			</button>
		</div>
	{/if}
</div>
