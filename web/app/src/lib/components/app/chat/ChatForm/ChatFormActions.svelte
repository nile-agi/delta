<script lang="ts">
	import { Square, ArrowUp, Wrench, Calendar, StickyNote, X } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import * as Tooltip from '$lib/components/ui/tooltip';
	import ChatFormActionFileAttachments from './ChatFormActionFileAttachments.svelte';
	import ChatFormActionRecord from './ChatFormActionRecord.svelte';
	import ChatFormModelSelector from './ChatFormModelSelector.svelte';
	import { config, updateConfig } from '$lib/stores/settings.svelte';
	import { agentToolsActive, selectedModelSupportsTools } from '$lib/stores/models.svelte';
	import { calendarWindow } from '$lib/stores/calendar-window.svelte';
	import { notesWindow } from '$lib/stores/notes-window.svelte';
	import type { FileTypeCategory } from '$lib/enums/files';

	// The stored flag is the user's intent; the click below is its only writer. Deriving it from the
	// model in an effect clobbered that choice on every remount (new chat, route swap).
	let toolsEnabled = $derived(config().useAgentTools === true);
	let modelSupportsTools = $derived(selectedModelSupportsTools());
	let toolsActive = $derived(agentToolsActive());
	let calendarMinimized = $derived(calendarWindow.state.open && calendarWindow.state.minimized);
	let notesMinimized = $derived(notesWindow.state.open && notesWindow.state.minimized);

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

	{#if calendarMinimized}
		<div class="group relative">
			<Tooltip.Root>
				<Tooltip.Trigger>
					<button
						type="button"
						class="flex h-8 w-8 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-accent hover:text-foreground"
						onclick={() => calendarWindow.restore()}
						aria-label="Restore Calendar"
					>
						<Calendar class="h-4 w-4" />
					</button>
				</Tooltip.Trigger>
				<Tooltip.Content><p>Restore Calendar</p></Tooltip.Content>
			</Tooltip.Root>
			<button
				type="button"
				class="absolute -top-1 -right-1 hidden h-3.5 w-3.5 items-center justify-center rounded-full bg-muted-foreground/60 text-background transition-colors hover:bg-destructive group-hover:flex"
				onclick={() => calendarWindow.close()}
				aria-label="Close Calendar"
			>
				<X class="h-2.5 w-2.5" />
			</button>
		</div>
	{/if}

	{#if notesMinimized}
		<div class="group relative">
			<Tooltip.Root>
				<Tooltip.Trigger>
					<button
						type="button"
						class="flex h-8 w-8 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-accent hover:text-foreground"
						onclick={() => notesWindow.restore()}
						aria-label="Restore Notes"
					>
						<StickyNote class="h-4 w-4" />
					</button>
				</Tooltip.Trigger>
				<Tooltip.Content><p>Restore Notes</p></Tooltip.Content>
			</Tooltip.Root>
			<button
				type="button"
				class="absolute -top-1 -right-1 hidden h-3.5 w-3.5 items-center justify-center rounded-full bg-muted-foreground/60 text-background transition-colors hover:bg-destructive group-hover:flex"
				onclick={() => notesWindow.close()}
				aria-label="Close Notes"
			>
				<X class="h-2.5 w-2.5" />
			</button>
		</div>
	{/if}

	<Tooltip.Root>
		<Tooltip.Trigger>
			<button
				type="button"
				class="flex h-8 w-8 items-center justify-center rounded-md transition-colors {toolsActive
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
