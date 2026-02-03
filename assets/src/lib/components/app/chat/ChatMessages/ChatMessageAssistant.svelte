<script lang="ts">
	import { ChatMessageThinkingBlock, MarkdownContent } from '$lib/components/app';
	import { useProcessingState } from '$lib/hooks/use-processing-state.svelte';
	import { isLoading } from '$lib/stores/chat.svelte';
	import { fade } from 'svelte/transition';
	import { Check, X, Box, ChevronDown, Wrench } from '@lucide/svelte';
	import { Button } from '$lib/components/ui/button';
	import { Checkbox } from '$lib/components/ui/checkbox';
	import * as DropdownMenu from '$lib/components/ui/dropdown-menu';
	import { INPUT_CLASSES } from '$lib/constants/input-classes';
	import ChatMessageActions from './ChatMessageActions.svelte';
	import ChatMessageStatistics from './ChatMessageStatistics.svelte';
	import Label from '$lib/components/ui/label/label.svelte';
	import { config } from '$lib/stores/settings.svelte';
	import { modelOptions, selectedModelId, selectModel } from '$lib/stores/models.svelte';
	import type { DatabaseMessageToolCall } from '$lib/types/database';
	import { copyToClipboard } from '$lib/utils/copy';
	import autoResizeTextarea from '$lib/utils/autoresize-textarea';

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
	let options = $derived(modelOptions());
	let activeModelId = $derived(selectedModelId());

	function getModelDisplayName(): string {
		const modelId = message.model;
		if (modelId) {
			const opt = options.find((m) => m.id === modelId || m.model === modelId);
			return (
				opt?.name ??
				modelId
					.split(/[/\\]/)
					.pop()
					?.replace(/\.gguf$/i, '') ??
				modelId
			);
		}
		if (activeModelId) {
			const opt = options.find((m) => m.id === activeModelId);
			return opt?.name ?? activeModelId;
		}
		return 'Unknown model';
	}

	async function handleModelSelect(optionId: string) {
		const current = message.model ?? activeModelId;
		if (optionId === current) return;
		try {
			await selectModel(optionId);
			onRegenerate();
		} catch (e) {
			console.error('Failed to switch model:', e);
		}
	}

	function formatToolCallBadge(toolCall: DatabaseMessageToolCall, index: number) {
		const callNumber = index + 1;
		const functionName = toolCall.name?.trim();
		const label = functionName || `Call #${callNumber}`;
		const payload: Record<string, unknown> = {};
		if (toolCall.name) payload.name = toolCall.name;
		if (toolCall.arguments) {
			try {
				payload.arguments = JSON.parse(toolCall.arguments);
			} catch {
				payload.arguments = toolCall.arguments;
			}
		}
		const copyValue = JSON.stringify(
			Object.keys(payload).length > 0
				? payload
				: { name: toolCall.name, arguments: toolCall.arguments },
			null,
			2
		);
		return { label, copyValue };
	}

	function handleCopyToolCall(payload: string) {
		void copyToClipboard(payload, 'Tool call copied to clipboard');
	}

	let t = $derived(message.timings);
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
				oninput={(e) => {
					autoResizeTextarea(e.currentTarget);
					onEditedContentChange?.(e.currentTarget.value);
				}}
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

	<div class="info my-6 grid gap-4 text-xs text-muted-foreground tabular-nums">
		{#if message.role === 'assistant'}
			<div class="inline-flex flex-wrap items-start gap-2">
				<DropdownMenu.Root>
					<DropdownMenu.Trigger
						class="inline-flex items-center gap-1.5 rounded-md border border-border/50 bg-muted/30 px-2.5 py-1.5 font-medium text-foreground hover:bg-muted/50 focus:ring-2 focus:ring-ring focus:outline-none"
					>
						<Box class="h-3.5 w-3.5 shrink-0" />
						<span class="max-w-[180px] truncate">{getModelDisplayName()}</span>
						<ChevronDown class="h-3.5 w-3.5 shrink-0" />
					</DropdownMenu.Trigger>
					<DropdownMenu.Content align="start" class="max-h-[min(60vh,320px)] overflow-y-auto">
						{#each options as option (option.id)}
							<DropdownMenu.Item
								onclick={() => handleModelSelect(option.id)}
								class="cursor-pointer"
							>
								<span class="truncate">{option.name}</span>
							</DropdownMenu.Item>
						{/each}
					</DropdownMenu.Content>
				</DropdownMenu.Root>

				{#if currentConfig.showMessageStats && t}
					<ChatMessageStatistics
						promptTokens={t.prompt_n ?? 0}
						promptMs={t.prompt_ms ?? 0}
						predictedTokens={t.predicted_n ?? 0}
						predictedMs={t.predicted_ms ?? 0}
					/>
				{/if}
			</div>
		{/if}

		{#if currentConfig.showToolCallLabels && message.tool_calls && message.tool_calls.length > 0}
			<span class="inline-flex flex-wrap items-center gap-2">
				<span class="inline-flex items-center gap-1">
					<Wrench class="h-3.5 w-3.5" />
					<span>Tool calls:</span>
				</span>
				{#each message.tool_calls as toolCall, index (toolCall.name ?? String(index))}
					{@const badge = formatToolCallBadge(toolCall, index)}
					<button
						type="button"
						class="tool-call-badge inline-flex cursor-pointer items-center gap-1 rounded-sm bg-muted-foreground/15 px-1.5 py-0.75 hover:bg-muted-foreground/25"
						title={badge.copyValue}
						aria-label="Copy tool call {badge.label}"
						onclick={() => handleCopyToolCall(badge.copyValue)}
					>
						<span class="max-w-[12rem] truncate">{badge.label}</span>
					</button>
				{/each}
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
			onContinue={currentConfig.enableContinueButton && !thinkingContent ? onContinue : undefined}
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

	.tool-call-badge {
		max-width: 12rem;
		white-space: nowrap;
		overflow: hidden;
		text-overflow: ellipsis;
	}
</style>
