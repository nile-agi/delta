<script lang="ts" module>
	// A file may finish loading after its note's editor has been remounted.
	const liveEditors = new Map<string, import('@tiptap/core').Editor>();
</script>

<script lang="ts">
	import { onMount, untrack } from 'svelte';
	import { Editor } from '@tiptap/core';
	import Document from '@tiptap/extension-document';
	import Paragraph from '@tiptap/extension-paragraph';
	import Text from '@tiptap/extension-text';
	import Bold from '@tiptap/extension-bold';
	import Italic from '@tiptap/extension-italic';
	import Underline from '@tiptap/extension-underline';
	import Strike from '@tiptap/extension-strike';
	import Highlight from '@tiptap/extension-highlight';
	import Heading from '@tiptap/extension-heading';
	import { ListKit } from '@tiptap/extension-list';
	import Blockquote from '@tiptap/extension-blockquote';
	import { TableKit } from '@tiptap/extension-table';
	import Image from '@tiptap/extension-image';
	import CodeBlock from '@tiptap/extension-code-block';
	import HardBreak from '@tiptap/extension-hard-break';
	import { UndoRedo, Gapcursor, Dropcursor } from '@tiptap/extensions';
	import { notesStore, type Note } from '$lib/stores/notes.svelte';
	import { positionPicker } from './popover';
	import { Attachment, LegacyBlock, prepareContent, serializeContent } from './content';
	import {
		Bold as BoldIcon,
		Italic as ItalicIcon,
		Underline as UnderlineIcon,
		Strikethrough,
		Highlighter,
		Heading1,
		Heading2,
		Heading3,
		List,
		ListOrdered,
		ListChecks,
		Quote,
		Table,
		Image as ImageIcon,
		Paperclip,
		Pencil,
		Smile,
		Undo2,
		Redo2,
		Download
	} from '@lucide/svelte';

	let { note }: { note: Note } = $props();
	let host: HTMLDivElement;
	let editor = $state<Editor>();
	let revision = $state(0);
	let dirty = $state(false);
	let emojiOpen = $state(false);
	let tableOpen = $state(false);
	let sketchOpen = $state(false);
	let rows = $state(3);
	let cols = $state(3);
	let canvas = $state<HTMLCanvasElement>(null!);
	let drawing = false;
	let fileInput: HTMLInputElement;
	let imageInput: HTMLInputElement;
	let fileError = $state('');
	let pending: { id: string; content: string } | null = null;
	let timeout: ReturnType<typeof setTimeout> | undefined;
	let disposed = false;
	let loadedContent = '';
	let lastSelection: { from: number; to: number } | null = null;
	const emojis = [
		'📝',
		'💡',
		'🔥',
		'⭐',
		'❤️',
		'⚡',
		'📌',
		'✅',
		'🔔',
		'🎨',
		'💼',
		'📚',
		'🎯',
		'🚀',
		'💻',
		'🏠'
	];
	const formats = [
		{
			name: 'Bold',
			icon: BoldIcon,
			mark: 'bold',
			run: () => editor?.chain().focus().toggleBold().run()
		},
		{
			name: 'Italic',
			icon: ItalicIcon,
			mark: 'italic',
			run: () => editor?.chain().focus().toggleItalic().run()
		},
		{
			name: 'Underline',
			icon: UnderlineIcon,
			mark: 'underline',
			run: () => editor?.chain().focus().toggleUnderline().run()
		},
		{
			name: 'Strikethrough',
			icon: Strikethrough,
			mark: 'strike',
			run: () => editor?.chain().focus().toggleStrike().run()
		},
		{
			name: 'Highlight',
			icon: Highlighter,
			mark: 'highlight',
			run: () => editor?.chain().focus().toggleHighlight().run()
		},
		{
			name: 'Heading 1',
			icon: Heading1,
			mark: 'heading',
			attrs: { level: 1 },
			run: () => editor?.chain().focus().toggleHeading({ level: 1 }).run()
		},
		{
			name: 'Heading 2',
			icon: Heading2,
			mark: 'heading',
			attrs: { level: 2 },
			run: () => editor?.chain().focus().toggleHeading({ level: 2 }).run()
		},
		{
			name: 'Heading 3',
			icon: Heading3,
			mark: 'heading',
			attrs: { level: 3 },
			run: () => editor?.chain().focus().toggleHeading({ level: 3 }).run()
		},
		{
			name: 'Bullet list',
			icon: List,
			mark: 'bulletList',
			run: () => editor?.chain().focus().toggleBulletList().run()
		},
		{
			name: 'Numbered list',
			icon: ListOrdered,
			mark: 'orderedList',
			run: () => editor?.chain().focus().toggleOrderedList().run()
		},
		{
			name: 'Checklist',
			icon: ListChecks,
			mark: 'taskList',
			run: () => editor?.chain().focus().toggleTaskList().run()
		},
		{
			name: 'Quote',
			icon: Quote,
			mark: 'blockquote',
			run: () => editor?.chain().focus().toggleBlockquote().run()
		}
	];
	let text = $derived.by(() => {
		void revision;
		return editor?.getText() || '';
	});
	let inTable = $derived.by(() => {
		void revision;
		return editor?.isActive('table') || false;
	});

	export function flush() {
		clearTimeout(timeout);
		timeout = undefined;
		if (pending) {
			notesStore.updateNote(pending.id, { content: pending.content });
			pending = null;
		}
		dirty = false;
	}
	function changed(instance: Editor) {
		loadedContent = serializeContent(instance.getHTML());
		pending = { id: note.id, content: loadedContent };
		dirty = true;
		clearTimeout(timeout);
		timeout = setTimeout(flush, 500);
	}
	function restoreSelection() {
		if (!editor) return;
		if (lastSelection) editor.commands.setTextSelection(lastSelection);
		else editor.commands.focus('end');
	}
	function insertEmoji(emoji: string) {
		restoreSelection();
		editor?.chain().focus().insertContent({ type: 'text', text: emoji }).run();
		emojiOpen = false;
	}
	function closePopovers(event: PointerEvent) {
		if (!(event.target as Element).closest('[data-emoji-picker]')) emojiOpen = false;
	}
	function keydown(event: KeyboardEvent) {
		if (event.key === 'Escape') {
			emojiOpen = false;
			tableOpen = false;
			sketchOpen = false;
		}
		if ((event.metaKey || event.ctrlKey) && event.key === 's') {
			event.preventDefault();
			flush();
		}
	}
	$effect(() => {
		const content = note.content;
		const instance = editor;
		untrack(() => {
			if (instance && !dirty && content !== loadedContent) {
				instance.commands.setContent(prepareContent(content), { emitUpdate: false });
				loadedContent = content;
			}
		});
	});
	function openDialog(node: HTMLDialogElement) {
		node.showModal();
	}
	onMount(() => {
		loadedContent = note.content;
		editor = new Editor({
			element: host,
			extensions: [
				Document,
				Paragraph,
				Text,
				Bold,
				Italic,
				Underline,
				Strike,
				Highlight,
				Heading.configure({ levels: [1, 2, 3] }),
				ListKit,
				Blockquote,
				TableKit,
				Image.configure({ allowBase64: true }),
				CodeBlock,
				HardBreak,
				UndoRedo,
				Gapcursor,
				Dropcursor,
				Attachment,
				LegacyBlock
			],
			content: prepareContent(note.content),
			editorProps: {
				attributes: {
					role: 'textbox',
					'aria-label': 'Note editor',
					'aria-multiline': 'true',
					class: 'note-prose',
					spellcheck: 'true'
				}
			},
			onUpdate: ({ editor: instance }) => changed(instance),
			onTransaction: ({ editor: instance }) => {
				revision++;
				if (instance.isFocused)
					lastSelection = { from: instance.state.selection.from, to: instance.state.selection.to };
			},
			onBlur: () => flush()
		});
		liveEditors.set(note.id, editor);
		return () => {
			flush();
			disposed = true;
			if (liveEditors.get(note.id) === editor) liveEditors.delete(note.id);
			editor?.destroy();
		};
	});

	async function attach(event: Event) {
		const input = event.target as HTMLInputElement;
		const files = Array.from(input.files || []);
		input.value = '';
		const targetId = note.id;
		restoreSelection();
		const position = editor?.state.selection.from;
		fileError = '';
		for (const file of files) {
			try {
				let content;
				if (file.type.startsWith('image/')) {
					const src = await new Promise<string>((resolve, reject) => {
						const reader = new FileReader();
						reader.onload = () => resolve(String(reader.result));
						reader.onerror = reject;
						reader.readAsDataURL(file);
					});
					content = { type: 'image', attrs: { src, alt: file.name } };
				} else if (
					file.type.startsWith('text/') ||
					/\.(txt|md|json|js|ts|html|css|py|csv|xml|yaml|yml)$/i.test(file.name)
				) {
					content = { type: 'attachment', attrs: { name: file.name, text: await file.text() } };
				} else {
					fileError = 'Choose an image or a text file.';
					continue;
				}
				const currentEditor = liveEditors.get(targetId);
				if (currentEditor && currentEditor !== editor) {
					currentEditor.commands.insertContentAt(currentEditor.state.doc.content.size, content);
				} else if (disposed || note.id !== targetId || !editor) {
					// Keep async imports attached to their originating note, even after navigation.
					const original = notesStore.notes.find((n) => n.id === targetId);
					if (original) {
						const el = document.createElement(content.type === 'image' ? 'img' : 'div');
						if (content.type === 'image') {
							el.setAttribute('src', content.attrs.src!);
							el.setAttribute('alt', file.name);
						} else {
							el.className = 'file-attachment';
							const h = document.createElement('div');
							h.className = 'file-header';
							h.textContent = file.name;
							const pre = document.createElement('pre');
							pre.textContent = content.attrs.text!;
							el.append(h, pre);
						}
						notesStore.updateNote(targetId, {
							content: prepareContent(original.content) + el.outerHTML
						});
					}
				} else {
					if (position !== undefined && files.length === 1)
						editor.commands.setTextSelection(Math.min(position, editor.state.doc.content.size));
					editor.chain().focus().insertContent(content).run();
				}
			} catch {
				fileError = `Unable to read ${file.name}.`;
			}
		}
	}
	function exportNote() {
		flush();
		const blob = new Blob([`# ${note.title}\n\n${editor?.getText() || ''}`], {
			type: 'text/plain'
		});
		const url = URL.createObjectURL(blob);
		const a = document.createElement('a');
		a.href = url;
		a.download = `${note.title || 'note'}.txt`;
		a.click();
		URL.revokeObjectURL(url);
	}
	function sketch(event: PointerEvent) {
		if (!drawing) return;
		const ctx = canvas.getContext('2d');
		if (!ctx) return;
		const rect = canvas.getBoundingClientRect();
		ctx.lineWidth = 3;
		ctx.lineCap = 'round';
		ctx.lineJoin = 'round';
		ctx.lineTo(
			((event.clientX - rect.left) * canvas.width) / rect.width,
			((event.clientY - rect.top) * canvas.height) / rect.height
		);
		ctx.stroke();
	}
</script>

<svelte:window
	onpointerdown={closePopovers}
	onkeydown={keydown}
	onpagehide={flush}
	onbeforeunload={flush}
/>
<div class="note-editor">
	<div class="toolbar" role="toolbar" aria-label="Note formatting">
		{#each formats as format}
			<button
				type="button"
				title={format.name}
				aria-label={format.name}
				aria-pressed={(revision >= 0 && editor?.isActive(format.mark, format.attrs)) || false}
				onpointerdown={(e) => e.preventDefault()}
				onclick={format.run}><format.icon size={16} /></button
			>
		{/each}
		<button
			title="Undo"
			aria-label="Undo"
			disabled={!editor || (revision >= 0 && !editor.can().undo())}
			onclick={() => editor?.chain().focus().undo().run()}><Undo2 size={16} /></button
		>
		<button
			title="Redo"
			aria-label="Redo"
			disabled={!editor || (revision >= 0 && !editor.can().redo())}
			onclick={() => editor?.chain().focus().redo().run()}><Redo2 size={16} /></button
		>
		<div class="emoji-anchor" data-emoji-picker>
			<button
				title="Add emoji"
				aria-label="Add emoji"
				aria-expanded={emojiOpen}
				onclick={() => (emojiOpen = !emojiOpen)}><Smile size={16} /></button
			>
			{#if emojiOpen}<div use:positionPicker class="emoji-picker" role="group" aria-label="Emoji">
					{#each emojis as emoji}<button onclick={() => insertEmoji(emoji)}>{emoji}</button>{/each}
				</div>{/if}
		</div>
		<button title="Insert table" aria-label="Insert table" onclick={() => (tableOpen = true)}
			><Table size={16} /></button
		>
		<button title="Insert image" aria-label="Insert image" onclick={() => imageInput.click()}
			><ImageIcon size={16} /></button
		>
		<button title="Attach file" aria-label="Attach file" onclick={() => fileInput.click()}
			><Paperclip size={16} /></button
		>
		<button title="Add sketch" aria-label="Add sketch" onclick={() => (sketchOpen = true)}
			><Pencil size={16} /></button
		>
		<button title="Export" aria-label="Export" onclick={exportNote}><Download size={16} /></button>
	</div>
	{#if inTable}<div class="toolbar table-actions" role="toolbar" aria-label="Table actions">
			<button onclick={() => editor?.chain().focus().addRowBefore().run()}>Row above</button><button
				onclick={() => editor?.chain().focus().addRowAfter().run()}>Row below</button
			>
			<button onclick={() => editor?.chain().focus().addColumnBefore().run()}>Column before</button
			><button onclick={() => editor?.chain().focus().addColumnAfter().run()}>Column after</button>
			<button onclick={() => editor?.chain().focus().deleteRow().run()}>Delete row</button><button
				onclick={() => editor?.chain().focus().deleteColumn().run()}>Delete column</button
			><button onclick={() => editor?.chain().focus().deleteTable().run()}>Delete table</button>
		</div>{/if}
	{#if fileError}<p role="alert">{fileError}</p>{/if}
	<div class="editor-scroll" bind:this={host}></div>
	<div class="status">
		<span
			>{text.trim() ? text.trim().split(/\s+/).length : 0} words · {Array.from(text).length} chars</span
		><span>{notesStore.saveError ? 'Not saved' : dirty ? 'Saving…' : 'Saved'}</span>
	</div>
</div>
<input bind:this={fileInput} type="file" hidden multiple onchange={attach} />
<input bind:this={imageInput} type="file" hidden multiple accept="image/*" onchange={attach} />
{#if tableOpen || sketchOpen}
	<dialog
		use:openDialog
		aria-label={tableOpen ? 'Insert table' : 'Add sketch'}
		class="editor-dialog"
		oncancel={() => {
			tableOpen = false;
			sketchOpen = false;
		}}
	>
		{#if tableOpen}
			<h3>Insert table</h3>
			<label>Rows <input type="number" min="1" max="20" bind:value={rows} /></label><label
				>Columns <input type="number" min="1" max="10" bind:value={cols} /></label
			>
			<button
				onclick={() => {
					restoreSelection();
					editor
						?.chain()
						.focus()
						.insertTable({
							rows: Math.max(1, Math.min(20, rows || 3)),
							cols: Math.max(1, Math.min(10, cols || 3)),
							withHeaderRow: false
						})
						.run();
					tableOpen = false;
				}}>Insert</button
			>
		{:else}
			<h3>Add sketch</h3>
			<canvas
				bind:this={canvas}
				width="600"
				height="350"
				aria-label="Drawing canvas"
				onpointerdown={(e) => {
					drawing = true;
					canvas.setPointerCapture(e.pointerId);
					canvas.getContext('2d')?.beginPath();
					sketch(e);
				}}
				onpointermove={sketch}
				onpointerup={() => (drawing = false)}
				onpointercancel={() => (drawing = false)}
			></canvas>
			<button onclick={() => canvas.getContext('2d')?.clearRect(0, 0, 600, 350)}>Clear</button>
			<button
				onclick={() => {
					restoreSelection();
					editor?.chain().focus().setImage({ src: canvas.toDataURL(), alt: 'Sketch' }).run();
					sketchOpen = false;
				}}>Insert sketch</button
			>
		{/if}
		<button
			onclick={() => {
				tableOpen = false;
				sketchOpen = false;
				editor?.commands.focus();
			}}>Cancel</button
		>
	</dialog>
{/if}

<style>
	.note-editor {
		display: flex;
		flex-direction: column;
		flex: 1;
		min-height: 0;
		min-width: 0;
	}
	.toolbar {
		display: flex;
		flex-wrap: wrap;
		align-items: center;
		gap: 2px;
		padding: 8px 12px;
		border-bottom: 1px solid var(--border);
		flex-shrink: 0;
	}
	button {
		display: inline-flex;
		align-items: center;
		justify-content: center;
		padding: 7px;
		border-radius: 5px;
		cursor: pointer;
	}
	button:hover,
	button[aria-pressed='true'] {
		background: var(--accent);
	}
	button:focus-visible {
		outline: 2px solid var(--ring);
		outline-offset: 1px;
	}
	button:disabled {
		opacity: 0.35;
		cursor: default;
	}
	.table-actions {
		font-size: 11px;
	}
	.emoji-anchor {
		position: relative;
	}
	.emoji-picker {
		position: fixed;
		z-index: 100000;
		display: grid;
		grid-template-columns: repeat(4, 1fr);
		width: 152px;
		padding: 6px;
		border: 1px solid var(--border);
		background: var(--popover);
		border-radius: 8px;
		box-shadow: 0 8px 24px #0002;
	}
	.editor-scroll {
		flex: 1;
		min-height: 0;
		overflow: auto;
		overscroll-behavior: contain;
	}
	.editor-scroll :global(.note-prose) {
		min-height: 100%;
		padding: 24px;
		outline: none;
		overflow-wrap: anywhere;
		line-height: 1.65;
	}
	.editor-scroll :global(p) {
		margin: 0.4em 0;
	}
	.editor-scroll :global(h1) {
		font-size: 2em;
		font-weight: 700;
	}
	.editor-scroll :global(h2) {
		font-size: 1.5em;
		font-weight: 700;
	}
	.editor-scroll :global(h3) {
		font-size: 1.2em;
		font-weight: 700;
	}
	.editor-scroll :global(ul) {
		list-style: disc;
		padding-left: 1.5em;
	}
	.editor-scroll :global(ol) {
		list-style: decimal;
		padding-left: 1.5em;
	}
	.editor-scroll :global(ul[data-type='taskList']) {
		list-style: none;
		padding-left: 0;
	}
	.editor-scroll :global(li[data-type='taskItem']) {
		display: flex;
		gap: 8px;
	}
	.editor-scroll :global(li[data-type='taskItem'] > div) {
		flex: 1;
		min-width: 0;
	}
	.editor-scroll :global(blockquote) {
		border-left: 3px solid var(--border);
		padding-left: 1em;
		color: var(--muted-foreground);
	}
	.editor-scroll :global(table) {
		border-collapse: collapse;
		width: 100%;
		table-layout: fixed;
	}
	.editor-scroll :global(td),
	.editor-scroll :global(th) {
		border: 1px solid var(--border);
		padding: 8px;
		vertical-align: top;
		position: relative;
	}
	.editor-scroll :global(.selectedCell) {
		background: var(--accent);
	}
	.editor-scroll :global(img) {
		max-width: 100%;
		height: auto;
	}
	.editor-scroll :global(pre) {
		white-space: pre-wrap;
		background: var(--muted);
		padding: 12px;
		border-radius: 6px;
	}
	.editor-scroll :global(.file-header) {
		font-size: 12px;
		font-weight: 600;
	}
	.editor-scroll :global(.legacy-block) {
		white-space: pre-wrap;
		border: 1px dashed var(--border);
		padding: 8px;
	}
	.status {
		display: flex;
		justify-content: space-between;
		flex-wrap: wrap;
		gap: 4px;
		padding: 6px 12px;
		font-size: 11px;
		color: var(--muted-foreground);
		border-top: 1px solid var(--border);
	}
	.editor-dialog::backdrop {
		background: #0005;
	}
	.editor-dialog {
		margin: auto;
		color: var(--foreground);
		max-width: calc(100% - 32px);
		max-height: 90vh;
		overflow: auto;
		background: var(--background);
		border: 1px solid var(--border);
		padding: 20px;
		border-radius: 12px;
		box-shadow: 0 12px 36px #0003;
	}
	.editor-dialog label {
		display: block;
		margin: 12px 0;
	}
	.editor-dialog input {
		width: 80px;
		padding: 4px;
		border: 1px solid var(--border);
	}
	canvas {
		display: block;
		width: 600px;
		max-width: 100%;
		background: white;
		touch-action: none;
		border: 1px solid var(--border);
	}
</style>
