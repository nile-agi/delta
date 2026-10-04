<script lang="ts">
	import { PROCESSING_INFO_TIMEOUT } from '$lib/constants/processing-info';
	import { useProcessingState } from '$lib/hooks/use-processing-state.svelte';
	import { isLoading, activeConversation } from '$lib/stores/chat.svelte';
	import { config } from '$lib/stores/settings.svelte';

	const processingState = useProcessingState();

	let isCurrentConversationLoading = $derived(isLoading());
	let processingDetails = $derived(processingState.getProcessingDetails());
	let showSlotsInfo = $derived(config().showMessageStats && isCurrentConversationLoading);

	// Show live processing details only while a reply is being generated.
	$effect(() => {
		if (activeConversation()?.id && isCurrentConversationLoading) {
			processingState.startMonitoring();
		} else {
			const timeout = setTimeout(() => processingState.stopMonitoring(), PROCESSING_INFO_TIMEOUT);
			return () => clearTimeout(timeout);
		}
	});
</script>

<div class="chat-processing-info-container" class:visible={showSlotsInfo}>
	<div class="chat-processing-info-content">
		{#each processingDetails as detail (detail)}
			<span class="chat-processing-info-detail">{detail}</span>
		{/each}
	</div>
</div>

<style>
	.chat-processing-info-container {
		position: sticky;
		top: 0;
		z-index: 10;
		padding: 1.5rem 1rem;
		opacity: 0;
		transform: translateY(50%);
		pointer-events: none;
		transition:
			opacity 300ms ease-out,
			transform 300ms ease-out;
	}

	.chat-processing-info-container.visible {
		opacity: 1;
		pointer-events: auto;
		transform: translateY(0);
	}

	.chat-processing-info-content {
		display: flex;
		flex-wrap: wrap;
		align-items: center;
		gap: 1rem;
		justify-content: center;
		max-width: 48rem;
		margin: 0 auto;
	}

	.chat-processing-info-detail {
		color: var(--muted-foreground);
		font-size: 0.75rem;
		padding: 0.25rem 0.75rem;
		background: var(--muted);
		border-radius: 0.375rem;
		font-family:
			ui-monospace, SFMono-Regular, 'SF Mono', Consolas, 'Liberation Mono', Menlo, monospace;
		white-space: nowrap;
	}

	@media (max-width: 768px) {
		.chat-processing-info-content {
			gap: 0.5rem;
		}

		.chat-processing-info-detail {
			font-size: 0.7rem;
			padding: 0.2rem 0.5rem;
		}
	}
</style>
