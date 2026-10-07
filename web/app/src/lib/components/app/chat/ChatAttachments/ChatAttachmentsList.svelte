<script lang="ts">
	import { ChatAttachmentImagePreview, ChatAttachmentFilePreview } from '$lib/components/app';
	import { FileTypeCategory } from '$lib/enums/files';
	import { getFileTypeCategory } from '$lib/utils/file-type';
	import ChatAttachmentPreviewDialog from './ChatAttachmentPreviewDialog.svelte';
	import { documentsService } from '$lib/services/documents';
	import { DatabaseStore } from '$lib/stores/database';
	import { toast } from 'svelte-sonner';

	interface Props {
		class?: string;
		// For ChatMessage - stored attachments
		attachments?: DatabaseMessageExtra[];
		/** The message the stored attachments belong to, so a change to them can be saved. */
		messageId?: string;
		readonly?: boolean;
		// For ChatForm - pending uploads
		onFileRemove?: (fileId: string) => void;
		uploadedFiles?: ChatUploadedFile[];
		// Image size customization
		imageClass?: string;
		imageHeight?: string;
		imageWidth?: string;
	}

	let {
		class: className = '',
		attachments = [],
		messageId,
		readonly = false,
		onFileRemove,
		uploadedFiles = $bindable([]),
		// Default to small size for form previews
		imageClass = '',
		imageHeight = 'h-24',
		imageWidth = 'w-auto'
	}: Props = $props();

	let displayItems = $derived(getDisplayItems());

	// Local view of "kept for all chats" so the pin answers at once, before the save lands.
	let keptOverride = $state<Record<number, boolean>>({});

	async function toggleKeep(index: number) {
		const attachment = attachments[index];
		if (attachment?.type !== 'documentFile' || !attachment.docId) return;
		const next = !(keptOverride[index] ?? attachment.library ?? false);
		try {
			await documentsService.setLibrary(attachment.docId, next);
			keptOverride[index] = next;
			if (messageId) {
				const saved: DatabaseMessageExtra[] = $state.snapshot(attachments);
				const extra = saved.map((a, i) => (i === index ? { ...a, library: next } : a));
				await DatabaseStore.updateMessage(messageId, { extra });
			}
			toast.success(
				next
					? `"${attachment.name}" can now be attached to any chat`
					: `"${attachment.name}" is kept in this chat only`
			);
		} catch (error) {
			console.error('Could not change where the file is kept:', error);
			toast.error(`Could not update "${attachment.name}".`);
		}
	}

	// Preview dialog state
	let previewDialogOpen = $state(false);
	let previewItem = $state<{
		uploadedFile?: ChatUploadedFile;
		attachment?: DatabaseMessageExtra;
		preview?: string;
		name?: string;
		type?: string;
		size?: number;
		textContent?: string;
	} | null>(null);

	function getDisplayItems() {
		const items: Array<{
			id: string;
			name: string;
			size?: number;
			preview?: string;
			type: string;
			isImage: boolean;
			uploadedFile?: ChatUploadedFile;
			attachment?: DatabaseMessageExtra;
			attachmentIndex?: number;
			textContent?: string;
			detail?: string;
			keepable?: boolean;
		}> = [];

		// Add uploaded files (ChatForm)
		for (const file of uploadedFiles) {
			items.push({
				id: file.id,
				name: file.name,
				size: file.size,
				preview: file.preview,
				type: file.type,
				isImage: getFileTypeCategory(file.type) === FileTypeCategory.IMAGE,
				uploadedFile: file,
				textContent: file.textContent
			});
		}

		// Add stored attachments (ChatMessage)
		for (const [index, attachment] of attachments.entries()) {
			if (attachment.type === 'imageFile') {
				items.push({
					id: `attachment-${index}`,
					name: attachment.name,
					preview: attachment.base64Url,
					type: 'image',
					isImage: true,
					attachment,
					attachmentIndex: index
				});
			} else if (attachment.type === 'textFile') {
				items.push({
					id: `attachment-${index}`,
					name: attachment.name,
					type: 'text',
					isImage: false,
					attachment,
					attachmentIndex: index,
					textContent: attachment.content
				});
			} else if (attachment.type === 'context') {
				// Legacy format from old webui - treat as text file
				items.push({
					id: `attachment-${index}`,
					name: attachment.name,
					type: 'text',
					isImage: false,
					attachment,
					attachmentIndex: index,
					textContent: attachment.content
				});
			} else if (attachment.type === 'audioFile') {
				items.push({
					id: `attachment-${index}`,
					name: attachment.name,
					type: attachment.mimeType || 'audio',
					isImage: false,
					attachment,
					attachmentIndex: index
				});
			} else if (attachment.type === 'documentFile') {
				const size =
					attachment.pages > 0
						? `${attachment.pages} ${attachment.pages === 1 ? 'page' : 'pages'}`
						: `${Math.max(1, Math.round(attachment.chars / 1000))}k characters`;
				items.push({
					id: `attachment-${index}`,
					name: attachment.name,
					type: attachment.mimeType,
					isImage: false,
					attachment,
					attachmentIndex: index,
					detail: attachment.docId ? `${size} · searchable` : size,
					keepable: !!attachment.docId
				});
			} else if (attachment.type === 'pdfFile') {
				items.push({
					id: `attachment-${index}`,
					name: attachment.name,
					type: 'application/pdf',
					isImage: false,
					attachment,
					attachmentIndex: index,
					textContent: attachment.content
				});
			}
		}

		return items;
	}

	type DisplayItem = ReturnType<typeof getDisplayItems>[number];

	function openPreview(item: DisplayItem, event?: Event) {
		if (event) {
			event.preventDefault();
			event.stopPropagation();
		}

		previewItem = {
			uploadedFile: item.uploadedFile,
			attachment: item.attachment,
			preview: item.preview,
			name: item.name,
			type: item.type,
			size: item.size,
			textContent: item.textContent
		};
		previewDialogOpen = true;
	}
</script>

{#if displayItems.length > 0}
	<div class="flex flex-wrap items-start {readonly ? 'justify-end' : ''} gap-3 {className}">
		{#each displayItems as item (item.id)}
			{#if item.isImage && item.preview}
				<ChatAttachmentImagePreview
					class="cursor-pointer"
					id={item.id}
					name={item.name}
					preview={item.preview}
					{readonly}
					onRemove={onFileRemove}
					height={imageHeight}
					width={imageWidth}
					{imageClass}
					onClick={(event) => openPreview(item, event)}
				/>
			{:else}
				<ChatAttachmentFilePreview
					class="cursor-pointer"
					id={item.id}
					name={item.name}
					type={item.type}
					size={item.size}
					{readonly}
					onRemove={onFileRemove}
					textContent={item.textContent}
					detail={item.detail}
					kept={item.attachmentIndex !== undefined &&
						(keptOverride[item.attachmentIndex] ??
							(item.attachment?.type === 'documentFile' && !!item.attachment.library))}
					onToggleKeep={item.keepable && readonly && item.attachmentIndex !== undefined
						? () => toggleKeep(item.attachmentIndex!)
						: undefined}
					onClick={(event) => openPreview(item, event)}
				/>
			{/if}
		{/each}
	</div>
{/if}

{#if previewItem}
	<ChatAttachmentPreviewDialog
		bind:open={previewDialogOpen}
		uploadedFile={previewItem.uploadedFile}
		attachment={previewItem.attachment}
		preview={previewItem.preview}
		name={previewItem.name}
		type={previewItem.type}
		size={previewItem.size}
		textContent={previewItem.textContent}
	/>
{/if}
