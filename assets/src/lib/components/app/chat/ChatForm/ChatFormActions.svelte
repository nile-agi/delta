<script lang="ts">
	import { Square, ArrowUp } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import ChatFormActionFileAttachments from './ChatFormActionFileAttachments.svelte';
	import ChatFormActionRecord from './ChatFormActionRecord.svelte';
	import ChatFormModelSelector from './ChatFormModelSelector.svelte';
	import type { FileTypeCategory } from '$lib/enums/files';

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
