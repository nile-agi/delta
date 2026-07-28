<script lang="ts">
	import { Square, ArrowUp, Wrench } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import * as Tooltip from '$lib/components/ui/tooltip';
	import ChatFormActionFileAttachments from './ChatFormActionFileAttachments.svelte';
	import ChatFormActionRecord from './ChatFormActionRecord.svelte';
	import ChatFormModelSelector from './ChatFormModelSelector.svelte';
	import { config, updateConfig } from '$lib/stores/settings.svelte';
	import {
		modelsLoading,
		selectedModelId,
		selectedModelSupportsTools
	} from '$lib/stores/models.svelte';
	import type { FileTypeCategory } from '$lib/enums/files';

	let toolsEnabled = $derived(config().useAgentTools === true);
	let modelSupportsTools = $derived(selectedModelSupportsTools());

	// Sync the toggle only when the model actually changes -- syncing on every render would clobber
	// the user's choice, and would persist `false` while the model list is still loading.
	let lastSyncedModelId: string | null = null;
	$effect(() => {
		const modelId = selectedModelId();
		if (modelsLoading() || !modelId || modelId === lastSyncedModelId) return;
		lastSyncedModelId = modelId;
		if (modelSupportsTools !== toolsEnabled) {
			updateConfig('useAgentTools', modelSupportsTools);
		}
	});

	interface Props {
		canSend?: boolean;
		class?: string;
		disabled?: boolean;
		hasModel?: boolean;
		isLoading?: boolean;
		isRecording?: boolean;
		recordingSupported?: boolean;
		showMicrophoneOnEmptyInput?: boolean;
		isEmpty?: boolean;
		openModelDropdownTrigger?: number;
		onRequestOpenModelDropdown?: () => void;
		onFileUpload?: (fileType?: FileTypeCategory) => void;
		onMicClick?: () => void;
		onStop?: () => void;
	}

	let {
		canSend = false,
		class: className = '',
		disabled = false,
		hasModel = true,
		isLoading = false,
		isRecording = false,
		recordingSupported = false,
		showMicrophoneOnEmptyInput = false,
		isEmpty = true,
		openModelDropdownTrigger = 0,
		onRequestOpenModelDropdown,
		onFileUpload,
		onMicClick,
		onStop
	}: Props = $props();

	let showSendButton = $derived(
		canSend || !(isEmpty && showMicrophoneOnEmptyInput && recordingSupported)
	);
	let sendDisabled = $derived(!canSend || disabled || isLoading);
	let sendBlockedNoModel = $derived(canSend && !hasModel);
</script>

<div class="flex w-full items-center gap-2 {className}">
	<ChatFormActionFileAttachments class="mr-auto" {disabled} {onFileUpload} />

	<Tooltip.Root>
		<Tooltip.Trigger>
			<button
				type="button"
				class="flex h-8 w-8 items-center justify-center rounded-md transition-colors {toolsEnabled && modelSupportsTools
					? 'bg-primary text-primary-foreground'
					: !modelSupportsTools
						? 'text-muted-foreground/40 cursor-not-allowed'
						: 'text-muted-foreground hover:text-foreground hover:bg-accent'}"
				onclick={() => {
					if (!modelSupportsTools) return;
					updateConfig('useAgentTools', !toolsEnabled);
				}}
				disabled={!modelSupportsTools}
			>
				<Wrench class="h-4 w-4" />
			</button>
		</Tooltip.Trigger>
		<Tooltip.Content>
			<p>
				{#if !modelSupportsTools}
					This model does not support agent tools
				{:else if toolsEnabled}
					Agent tools enabled (calendar)
				{:else}
					Enable agent tools
				{/if}
			</p>
		</Tooltip.Content>
	</Tooltip.Root>

	<ChatFormModelSelector class="shrink-0" openTrigger={openModelDropdownTrigger} />

	{#if isLoading}
		<Button
			type="button"
			onclick={onStop}
			class="h-8 w-8 bg-transparent p-0 hover:bg-destructive/20"
		>
			<span class="sr-only">Stop</span>
			<Square class="h-8 w-8 fill-destructive stroke-destructive" />
		</Button>
	{:else}
		<ChatFormActionRecord {disabled} {isLoading} {isRecording} {onMicClick} />

		{#if showSendButton}
			{#if sendBlockedNoModel}
				<Button
					type="button"
					class="h-8 w-8 rounded-full p-0"
					title="Please select a model first"
					onclick={onRequestOpenModelDropdown}
				>
					<span class="sr-only">Please select a model first</span>
					<ArrowUp class="h-12 w-12" />
				</Button>
			{:else}
				<Button type="submit" disabled={sendDisabled} class="h-8 w-8 rounded-full p-0">
					<span class="sr-only">Send</span>
					<ArrowUp class="h-12 w-12" />
				</Button>
			{/if}
		{/if}
	{/if}
</div>
