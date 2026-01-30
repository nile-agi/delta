<script lang="ts">
	import { ChatMessageThinkingBlock, MarkdownContent } from '$lib/components/app';
	import { useProcessingState } from '$lib/hooks/use-processing-state.svelte';
	import { isLoading } from '$lib/stores/chat.svelte';
	import { fade } from 'svelte/transition';
	import { Check, X, Gauge, Clock, WholeWord, ChartNoAxesColumn } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import { Checkbox } from '$lib/components/ui/checkbox';
	import { INPUT_CLASSES } from '$lib/constants/input-classes';
	import ChatMessageActions from './ChatMessageActions.svelte';
	import Label from '$lib/components/ui/label/label.svelte';
	import { config } from '$lib/stores/settings.svelte';

	interface Props {
		class?: string;
		deletionInfo: {
			totalCount: number;
			userMessages: number;
			assistantMessages: number;
			messageTypes: string[];
		} | null;
		editedContent?: string;
		isEditing?: boolean;
		message: DatabaseMessage;
		messageContent: string | undefined;
		onCancelEdit?: () => void;
		onCopy: () => void;
		onConfirmDelete: () => void;
		onDelete: () => void;
		onEdit?: () => void;
		onEditKeydown?: (event: KeyboardEvent) => void;
		onEditedContentChange?: (content: string) => void;
		onNavigateToSibling?: (siblingId: string) => void;
		onRegenerate: () => void;
		onContinue?: () => void;
		onSaveEdit?: () => void;
		onShowDeleteDialogChange: (show: boolean) => void;
		onShouldBranchAfterEditChange?: (value: boolean) => void;
		showDeleteDialog: boolean;
		shouldBranchAfterEdit?: boolean;
		siblingInfo?: ChatMessageSiblingInfo | null;
		textareaElement?: HTMLTextAreaElement;
		thinkingContent: string | null;
	}

	let {
		class: className = '',
		deletionInfo,
		editedContent = '',
		isEditing = false,
		message,
		messageContent,
		onCancelEdit,
		onConfirmDelete,
		onCopy,
		onDelete,
		onEdit,
		onEditKeydown,
		onEditedContentChange,
		onNavigateToSibling,
		onRegenerate,
		onContinue,
		onSaveEdit,
		onShowDeleteDialogChange,
		onShouldBranchAfterEditChange,
		showDeleteDialog,
		shouldBranchAfterEdit = false,
		siblingInfo = null,
		textareaElement = $bindable(),
		thinkingContent
	}: Props = $props();

	const processingState = useProcessingState();
	let currentConfig = $derived(config());
</script>

<div
	class="text-md group w-full leading-7.5 {className}"
	role="group"
	aria-label="Assistant message with actions"
>
	{#if thinkingContent}
		<ChatMessageThinkingBlock
			reasoningContent={thinkingContent}
			isStreaming={!message.timestamp}
			hasRegularContent={!!messageContent?.trim()}
		/>
	{/if}

	{#if message?.role === 'assistant' && isLoading() && !message?.content?.trim()}
		<div class="mt-6 w-full max-w-[48rem]" in:fade>
			<div class="processing-container">
				<span class="processing-text">
					{processingState.getProcessingMessage()}
				</span>
			</div>
		</div>
	{/if}

	{#if isEditing}
		<div class="w-full">
			<textarea
				bind:this={textareaElement}
				bind:value={editedContent}
				class="min-h-[50vh] w-full resize-y rounded-2xl px-3 py-2 text-sm {INPUT_CLASSES}"
				onkeydown={onEditKeydown}
				oninput={(e) => onEditedContentChange?.(e.currentTarget.value)}
				placeholder="Edit assistant message..."
			></textarea>

			<div class="mt-2 flex items-center justify-between">
				<div class="flex items-center space-x-2">
					<Checkbox
						id="branch-after-edit"
						bind:checked={shouldBranchAfterEdit}
						onCheckedChange={(checked) => onShouldBranchAfterEditChange?.(checked === true)}
					/>
					<Label for="branch-after-edit" class="cursor-pointer text-sm text-muted-foreground">
						Branch conversation after edit
					</Label>
				</div>
				<div class="flex gap-2">
					<Button class="h-8 px-3" onclick={onCancelEdit} size="sm" variant="outline">
						<X class="mr-1 h-3 w-3" />
						Cancel
					</Button>

					<Button class="h-8 px-3" onclick={onSaveEdit} disabled={!editedContent?.trim()} size="sm">
						<Check class="mr-1 h-3 w-3" />
						Save
					</Button>
				</div>
			</div>
		</div>
	{:else if message.role === 'assistant'}
		{#if config().disableReasoningFormat}
			<pre class="raw-output">{messageContent || ''}</pre>
		{:else}
			<MarkdownContent content={messageContent || ''} />
		{/if}
	{:else}
		<div class="text-sm whitespace-pre-wrap">
			{messageContent}
		</div>
	{/if}

	{#if currentConfig.showToolCallLabels && message.tool_calls?.length}
		<div class="mt-4 space-y-2 rounded-lg border border-border/50 bg-muted/20 p-3 text-xs">
			<span class="font-medium text-muted-foreground">Tool calls</span>
			{#each message.tool_calls as toolCall (toolCall.name ?? toolCall)}
				<div class="rounded border border-border/30 bg-background/50 p-2 font-mono">
					{#if toolCall.name}
						<div class="font-medium">{toolCall.name}</div>
					{/if}
					{#if toolCall.arguments}
						<pre class="mt-1 max-h-32 overflow-auto whitespace-pre-wrap break-words text-muted-foreground">{toolCall.arguments}</pre>
					{/if}
				</div>
			{/each}
		</div>
	{/if}

	<div class="info my-6 grid gap-4">
		{#if currentConfig.showMessageStats && message.timings && message.timings.predicted_n && message.timings.predicted_ms}
			{@const tokensPerSecond = (message.timings.predicted_n / message.timings.predicted_ms) * 1000}
			<span class="inline-flex items-center gap-2 text-xs text-muted-foreground">
				<span class="inline-flex items-center gap-1">
					<ChartNoAxesColumn class="h-3.5 w-3.5" />

					<span>Statistics:</span>
				</span>

				<div class="inline-flex flex-wrap items-center gap-2 text-xs text-muted-foreground">
					<span
						class="inline-flex items-center gap-1 rounded-sm bg-muted-foreground/15 px-1.5 py-0.75"
					>
						<Gauge class="h-3 w-3" />
						{tokensPerSecond.toFixed(2)} tokens/s
					</span>
					<span
						class="inline-flex items-center gap-1 rounded-sm bg-muted-foreground/15 px-1.5 py-0.75"
					>
						<WholeWord class="h-3 w-3" />
						{message.timings.predicted_n} tokens
					</span>
					<span
						class="inline-flex items-center gap-1 rounded-sm bg-muted-foreground/15 px-1.5 py-0.75"
					>
						<Clock class="h-3 w-3" />
						{(message.timings.predicted_ms / 1000).toFixed(2)}s
					</span>
				</div>
			</span>
		{/if}
	</div>

	{#if message.timestamp && !isEditing}
		<ChatMessageActions
			role="assistant"
			justify="start"
			actionsPosition="left"
			{siblingInfo}
			{showDeleteDialog}
			{deletionInfo}
			{onCopy}
			{onEdit}
			{onRegenerate}
			onContinue={currentConfig.enableContinueButton ? onContinue : undefined}
			{onDelete}
			{onConfirmDelete}
			{onNavigateToSibling}
			{onShowDeleteDialogChange}
		/>
	{/if}
</div>

<style>
	.processing-container {
		display: flex;
		flex-direction: column;
		align-items: flex-start;
		gap: 0.5rem;
	}

	.processing-text {
		background: linear-gradient(
			90deg,
			var(--muted-foreground),
			var(--foreground),
			var(--muted-foreground)
		);
		background-size: 200% 100%;
		background-clip: text;
		-webkit-background-clip: text;
		-webkit-text-fill-color: transparent;
		animation: shine 1s linear infinite;
		font-weight: 500;
		font-size: 0.875rem;
	}

	@keyframes shine {
		to {
			background-position: -200% 0;
		}
	}

	.raw-output {
		width: 100%;
		max-width: 48rem;
		margin-top: 1.5rem;
		padding: 1rem 1.25rem;
		border-radius: 1rem;
		background: hsl(var(--muted) / 0.3);
		color: var(--foreground);
		font-family:
			ui-monospace, SFMono-Regular, 'SF Mono', Monaco, 'Cascadia Code', 'Roboto Mono', Consolas,
			'Liberation Mono', Menlo, monospace;
		font-size: 0.875rem;
		line-height: 1.6;
		white-space: pre-wrap;
		word-break: break-word;
	}
</style>
