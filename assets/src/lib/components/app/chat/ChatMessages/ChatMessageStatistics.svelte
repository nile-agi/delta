<script lang="ts">
	import { BookOpen, Sparkles, Copy, Gauge, Clock, WholeWord } from '@lucide/svelte';
	import { copyToClipboard } from '$lib/utils/copy';
	import * as Tooltip from '$lib/components/ui/tooltip';

	/** Delay (ms) before tooltip shows — fast appearance on hover */
	const TOOLTIP_DELAY_MS = 150;

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
	const isLiveReading = $derived(isLive && liveState?.status === 'preparing');
	const isLiveGen = $derived(isLive && liveState?.status === 'generating');

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

	// When live, hide "0" until data arrives — show placeholder so stats feel real-time
	const displayPromptTokens = $derived(
		isLiveReading && effectivePromptTokens === 0 ? '—' : String(effectivePromptTokens)
	);
	const displayPromptTime = $derived(
		isLiveReading && effectivePromptMs === 0 ? '—s' : (effectivePromptMs / 1000).toFixed(2) + 's'
	);
	const displayPromptSpeed = $derived(
		isLiveReading && promptSpeed === 0 ? '—' : promptSpeed.toFixed(2) + ' tokens/s'
	);
	const displayGenTokens = $derived(
		isLiveGen && effectivePredictedTokens === 0 ? '—' : String(effectivePredictedTokens)
	);
	const displayGenTime = $derived(
		isLiveGen && effectivePredictedMs === 0 ? '—s' : (effectivePredictedMs / 1000).toFixed(2) + 's'
	);
	const displayGenSpeed = $derived(
		isLiveGen && genSpeed === 0 ? '—' : genSpeed.toFixed(2) + ' tokens/s'
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
		<Tooltip.Root delayDuration={TOOLTIP_DELAY_MS}>
			<Tooltip.Trigger>
				<button
					type="button"
					class="inline-flex items-center justify-center rounded px-2 py-1.5 transition {statsView ===
					'reading'
						? 'bg-background text-foreground shadow-sm'
						: 'text-muted-foreground hover:text-foreground'}"
					aria-label="Reading (prompt processing)"
					onclick={() => (statsView = 'reading')}
				>
					<BookOpen class="h-3.5 w-3.5 shrink-0" />
				</button>
			</Tooltip.Trigger>
			<Tooltip.Content>
				<p>Reading (prompt processing)</p>
			</Tooltip.Content>
		</Tooltip.Root>
		<Tooltip.Root delayDuration={TOOLTIP_DELAY_MS}>
			<Tooltip.Trigger>
				<button
					type="button"
					class="inline-flex items-center justify-center rounded px-2 py-1.5 transition {statsView ===
					'generation'
						? 'bg-background text-foreground shadow-sm'
						: 'text-muted-foreground hover:text-foreground'}"
					aria-label="Generation (token output)"
					onclick={() => (statsView = 'generation')}
				>
					<Sparkles class="h-3.5 w-3.5 shrink-0" />
				</button>
			</Tooltip.Trigger>
			<Tooltip.Content>
				<p>Generation (token output)</p>
			</Tooltip.Content>
		</Tooltip.Root>
	</div>

	{#if statsView === 'reading'}
		<div class="inline-flex flex-wrap items-center gap-2">
			<Tooltip.Root delayDuration={TOOLTIP_DELAY_MS}>
				<Tooltip.Trigger>
					<button
						type="button"
						class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
						aria-label={isLiveReading && effectivePromptTokens === 0
							? 'Prompt tokens: updating. Copy to clipboard'
							: `Prompt tokens: ${effectivePromptTokens}. Copy to clipboard`}
						onclick={() => copyStat(displayPromptTokens, 'Prompt tokens')}
					>
						<WholeWord class="h-3 w-3" />
						<span>{displayPromptTokens} tokens</span>
						<Copy class="h-3 w-3 opacity-70" />
					</button>
				</Tooltip.Trigger>
				<Tooltip.Content>
					<p>Copy prompt tokens</p>
				</Tooltip.Content>
			</Tooltip.Root>
			<Tooltip.Root delayDuration={TOOLTIP_DELAY_MS}>
				<Tooltip.Trigger>
					<button
						type="button"
						class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
						aria-label={isLiveReading && effectivePromptMs === 0
							? 'Prompt processing time: updating. Copy to clipboard'
							: `Prompt processing time: ${(effectivePromptMs / 1000).toFixed(2)} seconds. Copy to clipboard`}
						onclick={() => copyStat(displayPromptTime, 'Prompt processing time')}
					>
						<Clock class="h-3 w-3" />
						<span>{displayPromptTime}</span>
						<Copy class="h-3 w-3 opacity-70" />
					</button>
				</Tooltip.Trigger>
				<Tooltip.Content>
					<p>Copy prompt processing time</p>
				</Tooltip.Content>
			</Tooltip.Root>
			<Tooltip.Root delayDuration={TOOLTIP_DELAY_MS}>
				<Tooltip.Trigger>
					<button
						type="button"
						class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
						aria-label={isLiveReading && promptSpeed === 0
							? 'Prompt processing speed: updating. Copy to clipboard'
							: `Prompt processing speed: ${promptSpeed.toFixed(2)} tokens per second. Copy to clipboard`}
						onclick={() => copyStat(displayPromptSpeed, 'Prompt processing speed')}
					>
						<Gauge class="h-3 w-3" />
						<span>{displayPromptSpeed}</span>
						<Copy class="h-3 w-3 opacity-70" />
					</button>
				</Tooltip.Trigger>
				<Tooltip.Content>
					<p>Copy prompt processing speed</p>
				</Tooltip.Content>
			</Tooltip.Root>
		</div>
	{:else}
		<div class="inline-flex flex-wrap items-center gap-2">
			<Tooltip.Root delayDuration={TOOLTIP_DELAY_MS}>
				<Tooltip.Trigger>
					<button
						type="button"
						class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
						aria-label={isLiveGen && effectivePredictedTokens === 0
							? 'Generated tokens: updating. Copy to clipboard'
							: `Generated tokens: ${effectivePredictedTokens}. Copy to clipboard`}
						onclick={() => copyStat(displayGenTokens, 'Generated tokens')}
					>
						<WholeWord class="h-3 w-3" />
						<span>{displayGenTokens} tokens</span>
						<Copy class="h-3 w-3 opacity-70" />
					</button>
				</Tooltip.Trigger>
				<Tooltip.Content>
					<p>Generated tokens</p>
				</Tooltip.Content>
			</Tooltip.Root>
			<Tooltip.Root delayDuration={TOOLTIP_DELAY_MS}>
				<Tooltip.Trigger>
					<button
						type="button"
						class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
						aria-label={isLiveGen && effectivePredictedMs === 0
							? 'Generation time: updating. Copy to clipboard'
							: `Generation time: ${(effectivePredictedMs / 1000).toFixed(2)} seconds. Copy to clipboard`}
						onclick={() => copyStat(displayGenTime, 'Generation time')}
					>
						<Clock class="h-3 w-3" />
						<span>{displayGenTime}</span>
						<Copy class="h-3 w-3 opacity-70" />
					</button>
				</Tooltip.Trigger>
				<Tooltip.Content>
					<p>Generation time</p>
				</Tooltip.Content>
			</Tooltip.Root>
			<Tooltip.Root delayDuration={TOOLTIP_DELAY_MS}>
				<Tooltip.Trigger>
					<button
						type="button"
						class="inline-flex items-center gap-1 rounded-sm bg-muted/40 px-1.5 py-1 hover:bg-muted/60"
						aria-label={isLiveGen && genSpeed === 0
							? 'Generation speed: updating. Copy to clipboard'
							: `Generation speed: ${genSpeed.toFixed(2)} tokens per second. Copy to clipboard`}
						onclick={() => copyStat(displayGenSpeed, 'Generation speed')}
					>
						<Gauge class="h-3 w-3" />
						<span>{displayGenSpeed}</span>
						<Copy class="h-3 w-3 opacity-70" />
					</button>
				</Tooltip.Trigger>
				<Tooltip.Content>
					<p>Generation speed</p>
				</Tooltip.Content>
			</Tooltip.Root>
		</div>
	{/if}
</div>
