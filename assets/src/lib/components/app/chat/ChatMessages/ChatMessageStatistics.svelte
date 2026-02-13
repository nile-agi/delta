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
		/** When backend doesn't send timings, use elapsed time and content length to show at least something */
		streamFallback?: {
			startTimeMs: number;
			contentLength: number;
			/** When set, Generation time = now - this (excludes prompt phase) */
			generationStartTimeMs?: number;
		} | null;
	}

	let {
		promptTokens,
		promptMs,
		predictedTokens,
		predictedMs,
		liveState = null,
		streamFallback = null
	}: Props = $props();

	let statsView = $state<'reading' | 'generation'>('generation');
	/** Ticks every 500ms while streaming so fallback elapsed time updates */
	let nowMs = $state(Date.now());

	$effect(() => {
		if (!streamFallback) return;
		const interval = setInterval(() => {
			nowMs = Date.now();
		}, 500);
		return () => clearInterval(interval);
	});

	// Use live state when active; otherwise use final timings from props. Fallback to client-side when no server timings.
	const hasServerLiveState = $derived(liveState != null && liveState.status !== 'idle');
	const useFallback = $derived(
		streamFallback != null && !hasServerLiveState
	);
	// When streaming but no content yet (e.g. PDF/image prompt processing), show Reading with elapsed time
	const useReadingFallback = $derived(
		useFallback && streamFallback != null && streamFallback.contentLength === 0
	);
	const useGenerationFallback = $derived(
		useFallback && streamFallback != null && streamFallback.contentLength > 0
	);
	const isLive = $derived(hasServerLiveState || useFallback);
	const isLiveReading = $derived(
		hasServerLiveState && liveState?.status === 'preparing'
	);
	const isLiveGen = $derived(
		hasServerLiveState && liveState?.status === 'generating'
	);

	// Reading fallback: elapsed time + estimated tokens/speed so all three stream (backend may not send prompt_progress)
	const readingFallbackElapsedMs = $derived(
		useReadingFallback && streamFallback ? nowMs - streamFallback.startTimeMs : 0
	);
	/** Approximate prompt tokens during reading when backend doesn't send (e.g. PDF): ~60 tokens/s */
	const READING_FALLBACK_TOKENS_PER_SEC = 60;
	const readingFallbackEstTokens = $derived(
		useReadingFallback && streamFallback
			? Math.max(0, Math.round((readingFallbackElapsedMs / 1000) * READING_FALLBACK_TOKENS_PER_SEC))
			: 0
	);
	// Generation fallback: elapsed since first token (not stream start), and approximate token count (~4 chars per token)
	const generationStartMs = $derived(
		streamFallback?.generationStartTimeMs ?? streamFallback?.startTimeMs ?? 0
	);
	const fallbackElapsedMs = $derived(
		useGenerationFallback && streamFallback ? nowMs - generationStartMs : 0
	);
	const fallbackApproxTokens = $derived(
		useGenerationFallback && streamFallback
			? Math.max(0, Math.round(streamFallback.contentLength / 4))
			: 0
	);

	const effectivePromptTokens = $derived(
		isLiveReading
			? (liveState!.promptProgressProcessed ?? liveState!.promptTokens ?? 0)
			: useReadingFallback
				? readingFallbackEstTokens
				: promptTokens
	);
	const effectivePromptMs = $derived(
		isLiveReading && liveState?.promptProgressTimeMs != null
			? liveState.promptProgressTimeMs
			: useReadingFallback
				? readingFallbackElapsedMs
				: promptMs
	);
	const effectivePredictedTokens = $derived(
		useGenerationFallback
			? fallbackApproxTokens
			: isLiveGen
				? liveState!.tokensDecoded
				: predictedTokens
	);
	// Prefer client elapsed for generation time when we have generationStartTimeMs so it streams in real time (every 500ms)
	const generationClientElapsedMs = $derived(
		streamFallback?.generationStartTimeMs != null ? nowMs - streamFallback.generationStartTimeMs : null
	);
	const effectivePredictedMs = $derived(
		useGenerationFallback
			? fallbackElapsedMs
			: isLiveGen && generationClientElapsedMs != null
				? generationClientElapsedMs
				: isLiveGen
					? liveState!.generationTimeMs ??
							(liveState!.tokensPerSecond && liveState!.tokensPerSecond > 0
								? (liveState!.tokensDecoded / liveState!.tokensPerSecond) * 1000
								: 0)
					: predictedMs
	);

	const promptSpeed = $derived(
		effectivePromptMs > 0 && effectivePromptTokens > 0
			? (effectivePromptTokens / effectivePromptMs) * 1000
			: (liveState?.promptTokensPerSecond ?? 0)
	);
	const genSpeed = $derived(
		effectivePredictedMs > 0
			? (effectivePredictedTokens / effectivePredictedMs) * 1000
			: (liveState?.tokensPerSecond ?? 0)
	);

	// When live, hide "0" until data arrives (reading fallback shows estimated tokens)
	const displayPromptTokens = $derived(
		(isLiveReading || useReadingFallback) && effectivePromptTokens === 0
			? '—'
			: String(effectivePromptTokens)
	);
	const displayPromptTime = $derived(
		(isLiveReading || useReadingFallback) && effectivePromptMs === 0
			? '—s'
			: (effectivePromptMs / 1000).toFixed(2) + 's'
	);
	const displayPromptSpeed = $derived(
		(isLiveReading || useReadingFallback) && promptSpeed === 0
			? '—'
			: promptSpeed.toFixed(2) + ' tokens/s'
	);
	const displayGenTokens = $derived(
		useGenerationFallback && fallbackApproxTokens === 0
			? '—'
			: isLiveGen && effectivePredictedTokens === 0
				? '—'
				: String(effectivePredictedTokens)
	);
	const displayGenTime = $derived(
		!useGenerationFallback && isLiveGen && effectivePredictedMs === 0
			? '—s'
			: (effectivePredictedMs / 1000).toFixed(2) + 's'
	);
	const displayGenSpeed = $derived(
		!useGenerationFallback && isLiveGen && genSpeed === 0 ? '—' : genSpeed.toFixed(2) + ' tokens/s'
	);

	// When live, sync tab to current phase: reading fallback or server preparing → Reading; else Generation
	$effect(() => {
		if (useReadingFallback || (liveState?.status === 'preparing')) {
			statsView = 'reading';
			return;
		}
		if (useGenerationFallback || liveState?.status === 'generating') {
			statsView = 'generation';
			return;
		}
		if (!liveState || liveState.status === 'idle') return;
	});

	// During prompt processing only Reading is available; during generation only Generation is available
	const isInReadingPhase = $derived(isLiveReading || useReadingFallback);
	const isInGenerationPhase = $derived(isLiveGen || useGenerationFallback);
	const disableReadingButton = $derived(isInGenerationPhase);
	const disableGenerationButton = $derived(isInReadingPhase);

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
						: 'text-muted-foreground hover:text-foreground'} {disableReadingButton
						? 'cursor-not-allowed opacity-50'
						: ''}"
					aria-label="Reading (prompt processing)"
					aria-disabled={disableReadingButton}
					disabled={disableReadingButton}
					onclick={() => !disableReadingButton && (statsView = 'reading')}
				>
					<BookOpen class="h-3.5 w-3.5 shrink-0" />
				</button>
			</Tooltip.Trigger>
			<Tooltip.Content>
				<p>
					{disableReadingButton
						? 'Reading (prompt processing) — available after generation'
						: 'Reading (prompt processing)'}
				</p>
			</Tooltip.Content>
		</Tooltip.Root>
		<Tooltip.Root delayDuration={TOOLTIP_DELAY_MS}>
			<Tooltip.Trigger>
				<button
					type="button"
					class="inline-flex items-center justify-center rounded px-2 py-1.5 transition {statsView ===
					'generation'
						? 'bg-background text-foreground shadow-sm'
						: 'text-muted-foreground hover:text-foreground'} {disableGenerationButton
						? 'cursor-not-allowed opacity-50'
						: ''}"
					aria-label="Generation (token output)"
					aria-disabled={disableGenerationButton}
					disabled={disableGenerationButton}
					onclick={() => !disableGenerationButton && (statsView = 'generation')}
				>
					<Sparkles class="h-3.5 w-3.5 shrink-0" />
				</button>
			</Tooltip.Trigger>
			<Tooltip.Content>
				<p>
					{disableGenerationButton
						? 'Generation (token output) — available after prompt is read'
						: 'Generation (token output)'}
				</p>
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
