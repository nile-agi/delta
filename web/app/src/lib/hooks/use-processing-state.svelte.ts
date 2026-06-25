import { slotsService } from '$lib/services';
import { config } from '$lib/stores/settings.svelte';

export interface UseProcessingStateReturn {
	readonly processingState: ApiProcessingState | null;
	getProcessingDetails(): string[];
	getProcessingMessage(): string;
	shouldShowDetails(): boolean;
	startMonitoring(): Promise<void>;
	stopMonitoring(): void;
}

/**
 * useProcessingState - Reactive processing state hook
 *
 * This hook provides reactive access to the processing state of the server.
 * It subscribes to timing data updates from the slots service and provides
 * formatted processing details for UI display.
 *
 * **Features:**
 * - Real-time processing state monitoring
 * - Context and output token tracking
 * - Tokens per second calculation
 * - Graceful degradation when slots endpoint unavailable
 * - Automatic cleanup on component unmount
 *
 * @returns Hook interface with processing state and control methods
 */
export function useProcessingState(): UseProcessingStateReturn {
	let isMonitoring = $state(false);
	let processingState = $state<ApiProcessingState | null>(null);
	let lastKnownState = $state<ApiProcessingState | null>(null);
	let unsubscribe: (() => void) | null = null;

	async function startMonitoring(): Promise<void> {
		if (isMonitoring) return;

		isMonitoring = true;

		unsubscribe = slotsService.subscribe((state) => {
			processingState = state;
			if (state) {
				lastKnownState = state;
			} else {
				lastKnownState = null;
			}
		});

		try {
			const currentState = await slotsService.getCurrentState();

			if (currentState) {
				processingState = currentState;
				lastKnownState = currentState;
			}

			if (slotsService.isStreaming()) {
				slotsService.startStreaming();
			}
		} catch (error) {
			console.warn('Failed to start slots monitoring:', error);
			// Continue without slots monitoring - graceful degradation
		}
	}

	function stopMonitoring(): void {
		if (!isMonitoring) return;

		isMonitoring = false;

		// Only clear processing state if keepStatsVisible is disabled
		// This preserves the last known state for display when stats should remain visible
		const currentConfig = config();
		if (!currentConfig.keepStatsVisible) {
			processingState = null;
		} else if (lastKnownState) {
			// Keep the last known state visible when keepStatsVisible is enabled
			processingState = lastKnownState;
		}

		if (unsubscribe) {
			unsubscribe();
			unsubscribe = null;
		}
	}

	function getProcessingMessage(): string {
		if (!processingState) {
			return 'Processing 0% (ETA: —s)';
		}

		switch (processingState.status) {
			case 'initializing':
				return 'Processing 0% (ETA: —s)';
			case 'preparing': {
				// Always use "Processing X% (ETA: Ys)" for consistency (normal chat, PDF, image, text upload, etc.)
				const percent =
					processingState.progressPercent !== undefined
						? processingState.progressPercent
						: 0;
				const etaSeconds = calculateETA(processingState);
				const etaStr =
					etaSeconds !== null && etaSeconds > 0 ? `${Math.round(etaSeconds)}s` : '—s';
				return `Processing ${percent}% (ETA: ${etaStr})`;
			}
			case 'generating':
				if (processingState.tokensDecoded > 0) {
					return `Generating... (${processingState.tokensDecoded} tokens)`;
				}
				return 'Generating...';
			default:
				return 'Processing 0% (ETA: —s)';
		}
	}

	/**
	 * Calculate ETA (Estimated Time to Arrival) in seconds based on current progress
	 * Returns null if calculation is not possible
	 */
	function calculateETA(state: ApiProcessingState): number | null {
		if (
			state.progressPercent === undefined ||
			state.progressPercent <= 0 ||
			state.progressPercent >= 100 ||
			!state.promptProgressTimeMs ||
			state.promptProgressTimeMs <= 0
		) {
			return null;
		}

		// Calculate ETA: if we've processed X% in Y seconds, remaining time = (Y / X) * (100 - X)
		const elapsedSeconds = state.promptProgressTimeMs / 1000;
		const progressDecimal = state.progressPercent / 100;
		const remainingProgress = 1 - progressDecimal;

		if (progressDecimal <= 0) {
			return null;
		}

		const estimatedTotalTime = elapsedSeconds / progressDecimal;
		const etaSeconds = estimatedTotalTime - elapsedSeconds;

		return etaSeconds > 0 ? etaSeconds : null;
	}

	function getProcessingDetails(): string[] {
		// Use current processing state or fall back to last known state
		const stateToUse = processingState || lastKnownState;
		if (!stateToUse) {
			return [];
		}

		const details: string[] = [];
		const currentConfig = config(); // Get fresh config each time

		// During prompt processing (preparing), show prompt-specific stats first
		if (stateToUse.status === 'preparing') {
			// Show prompt tokens if available
			if (stateToUse.promptTokens !== undefined && stateToUse.promptTokens > 0) {
				details.push(`${stateToUse.promptTokens} tokens`);
			}

			// Show elapsed time for prompt processing
			if (stateToUse.promptProgressTimeMs !== undefined && stateToUse.promptProgressTimeMs > 0) {
				const elapsedSeconds = (stateToUse.promptProgressTimeMs / 1000).toFixed(2);
				details.push(`${elapsedSeconds}s`);
			}

			// Show prompt processing speed (tokens per second)
			if (
				stateToUse.promptTokensPerSecond !== undefined &&
				stateToUse.promptTokensPerSecond > 0
			) {
				details.push(`${stateToUse.promptTokensPerSecond.toFixed(2)} tokens/s`);
			}
		}

		// During generation, show generation-specific stats first
		if (stateToUse.status === 'generating') {
			// Show generated tokens
			if (stateToUse.tokensDecoded > 0) {
				details.push(`${stateToUse.tokensDecoded} tokens`);
			}

			// Show elapsed time for generation
			// Use predicted_ms if available, otherwise calculate from tokensPerSecond
			let elapsedSeconds: string | null = null;
			if (stateToUse.generationTimeMs !== undefined && stateToUse.generationTimeMs > 0) {
				elapsedSeconds = (stateToUse.generationTimeMs / 1000).toFixed(2);
			} else if (
				stateToUse.tokensDecoded > 0 &&
				stateToUse.tokensPerSecond &&
				stateToUse.tokensPerSecond > 0
			) {
				// Calculate elapsed time from tokens and tokensPerSecond
				const calculatedMs = (stateToUse.tokensDecoded / stateToUse.tokensPerSecond) * 1000;
				elapsedSeconds = (calculatedMs / 1000).toFixed(2);
			}

			if (elapsedSeconds) {
				details.push(`${elapsedSeconds}s`);
			}

			// Show generation speed (tokens per second)
			if (
				currentConfig.showTokensPerSecond &&
				stateToUse.tokensPerSecond &&
				stateToUse.tokensPerSecond > 0
			) {
				details.push(`${stateToUse.tokensPerSecond.toFixed(2)} tokens/s`);
			}
		}

		// Always show context info when we have valid data
		if (stateToUse.contextUsed >= 0 && stateToUse.contextTotal > 0) {
			const contextPercent = Math.round((stateToUse.contextUsed / stateToUse.contextTotal) * 100);

			details.push(
				`Context: ${stateToUse.contextUsed}/${stateToUse.contextTotal} (${contextPercent}%)`
			);
		}

		if (stateToUse.outputTokensUsed > 0) {
			// Handle infinite max_tokens (-1) case
			if (stateToUse.outputTokensMax <= 0) {
				details.push(`Output: ${stateToUse.outputTokensUsed}/∞`);
			} else {
				const outputPercent = Math.round(
					(stateToUse.outputTokensUsed / stateToUse.outputTokensMax) * 100
				);

				details.push(
					`Output: ${stateToUse.outputTokensUsed}/${stateToUse.outputTokensMax} (${outputPercent}%)`
				);
			}
		}

		if (stateToUse.speculative) {
			details.push('Speculative decoding enabled');
		}

		return details;
	}

	function shouldShowDetails(): boolean {
		return processingState !== null && processingState.status !== 'idle';
	}

	return {
		get processingState() {
			return processingState;
		},
		getProcessingDetails,
		getProcessingMessage,
		shouldShowDetails,
		startMonitoring,
		stopMonitoring
	};
}
