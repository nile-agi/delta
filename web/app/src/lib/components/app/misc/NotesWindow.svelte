<script lang="ts">
	import { onDestroy } from 'svelte';
	import { notesStore, type Note } from '$lib/stores/notes.svelte';
	import { notesWindow } from '$lib/stores/notes-window.svelte';
	import { Button } from '$lib/components/ui/button/index.js';
	import { Input } from '$lib/components/ui/input/index.js';
	import { ScrollArea } from '$lib/components/ui/scroll-area/index.js';
	import {
		Plus, Trash2, FileText, Bold, Italic, Underline, Highlighter,
		Type, Heading1, Heading2, Heading3, Text, Code, List,
		ListOrdered, ListChecks, Quote, Table, Image as ImageIcon, Paperclip,
		Pencil, Share2, MoreHorizontal, Pin, Palette, Smile,
		Download, Printer, X, Minus, ChevronDown, Sparkles,
		ArrowUp, ArrowDown, ArrowLeft, ArrowRight, Trash, GripVertical
	} from '@lucide/svelte';
	import FloatingWindow from './FloatingWindow.svelte';

	let search = $state('');
	let editorRef = $state<HTMLDivElement | null>(null);
	let showStyleMenu = $state(false);
	let showMoreMenu = $state(false);
	let showTableDialog = $state(false);
	let showSketchModal = $state(false);
	let showColorPicker = $state(false);
	let showEmojiPicker = $state(false);
	let showExportToast = $state(false);
	let exportToastMsg = $state('');
	let tableRows = $state(3);
	let tableCols = $state(3);
	let wordCount = $state(0);
	let charCount = $state(0);
	let lastSaved = $state('');
	let sketchCanvas = $state<HTMLCanvasElement | null>(null);
	let isDrawing = $state(false);
	let sketchContext = $state<CanvasRenderingContext2D | null>(null);
	let currentTable = $state<HTMLTableElement | null>(null);
	let fileInput = $state<HTMLInputElement | null>(null);
	let mediaInput = $state<HTMLInputElement | null>(null);
	let isUpdatingFromEditor = $state(false);
	let saveTimeout: ReturnType<typeof setTimeout> | null = null;
	let exportToastTimeout: ReturnType<typeof setTimeout> | null = null;

	let tableDlgX = $state(0);
	let tableDlgY = $state(0);
	let tableDlgDragging = $state(false);
	let tableDlgDragOffX = $state(0);
	let tableDlgDragOffY = $state(0);
	let sketchDlgX = $state(0);
	let sketchDlgY = $state(0);
	let sketchDlgDragging = $state(false);
	let sketchDlgDragOffX = $state(0);
	let sketchDlgDragOffY = $state(0);

	const emojis = ['📝', '💡', '🔥', '⭐', '❤️', '⚡', '📌', '✅', '🔔', '🎨', '💼', '📚', '🎯', '🚀', '💻', '🏠', '❓', '💰'];

	const noteColors = [
		{ name: 'Default', value: null, border: '' },
		{ name: 'Red', value: 'red', border: 'border-l-red-400' },
		{ name: 'Orange', value: 'orange', border: 'border-l-orange-400' },
		{ name: 'Yellow', value: 'yellow', border: 'border-l-yellow-400' },
		{ name: 'Green', value: 'green', border: 'border-l-green-400' },
		{ name: 'Blue', value: 'blue', border: 'border-l-blue-400' },
		{ name: 'Purple', value: 'purple', border: 'border-l-purple-400' },
		{ name: 'Pink', value: 'pink', border: 'border-l-pink-400' },
	];

	let filteredNotes = $derived(
		notesStore.notes
			.filter(n =>
				n.title.toLowerCase().includes(search.toLowerCase()) ||
				(n.content || '').toLowerCase().includes(search.toLowerCase())
			)
			.sort((a, b) => {
				if (a.pinned && !b.pinned) return -1;
				if (!a.pinned && b.pinned) return 1;
				return b.updatedAt - a.updatedAt;
			})
	);

	let activeNote = $derived(notesStore.activeNote);

	function centerDialog() {
		tableDlgX = Math.max(20, window.innerWidth / 2 - 160);
		tableDlgY = Math.max(20, window.innerHeight / 2 - 120);
	}

	function onTableDragStart(e: PointerEvent) {
		if ((e.target as HTMLElement).closest('button, input')) return;
		e.preventDefault();
		tableDlgDragging = true;
		tableDlgDragOffX = e.clientX - tableDlgX;
		tableDlgDragOffY = e.clientY - tableDlgY;
		(e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
	}

	function onTableDragMove(e: PointerEvent) {
		if (!tableDlgDragging) return;
		tableDlgX = Math.max(0, Math.min(e.clientX - tableDlgDragOffX, window.innerWidth - 320));
		tableDlgY = Math.max(0, Math.min(e.clientY - tableDlgDragOffY, window.innerHeight - 200));
	}

	function onTableDragEnd(e: PointerEvent) {
		tableDlgDragging = false;
		const el = e.currentTarget as HTMLElement;
		if (el.hasPointerCapture(e.pointerId)) el.releasePointerCapture(e.pointerId);
	}

	function onSketchDragStart(e: PointerEvent) {
		if ((e.target as HTMLElement).closest('button, input, canvas')) return;
		e.preventDefault();
		sketchDlgDragging = true;
		sketchDlgDragOffX = e.clientX - sketchDlgX;
		sketchDlgDragOffY = e.clientY - sketchDlgY;
		(e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
	}

	function onSketchDragMove(e: PointerEvent) {
		if (!sketchDlgDragging) return;
		sketchDlgX = Math.max(0, Math.min(e.clientX - sketchDlgDragOffX, window.innerWidth - 600));
		sketchDlgY = Math.max(0, Math.min(e.clientY - sketchDlgDragOffY, window.innerHeight - 400));
	}

	function onSketchDragEnd(e: PointerEvent) {
		sketchDlgDragging = false;
		const el = e.currentTarget as HTMLElement;
		if (el.hasPointerCapture(e.pointerId)) el.releasePointerCapture(e.pointerId);
	}

	function updateCounts() {
		if (!editorRef) return;
		const text = editorRef.innerText || '';
		charCount = text.length;
		wordCount = text.trim() === '' ? 0 : text.trim().split(/\s+/).length;
	}

	function doSave() {
		if (!editorRef || !activeNote) return;
		isUpdatingFromEditor = true;
		const html = editorRef.innerHTML;
		notesStore.updateNote(activeNote.id, { content: html });
		lastSaved = new Date().toLocaleTimeString();
		updateCounts();
		setTimeout(() => { isUpdatingFromEditor = false; }, 0);
	}

	function updateContentFromEditor() {
		if (!editorRef || !activeNote) return;
		if (saveTimeout) clearTimeout(saveTimeout);
		saveTimeout = setTimeout(() => doSave(), 250);
	}

	function setupEditor(node: HTMLDivElement) {
		const observer = new MutationObserver(() => updateContentFromEditor());
		observer.observe(node, { childList: true, subtree: true, characterData: true, attributes: true });
		return { destroy() { observer.disconnect(); } };
	}

	$effect(() => {
		if (activeNote && editorRef && !isUpdatingFromEditor) {
			const stored = activeNote.content || '';
			const current = editorRef.innerHTML;
			if (stored !== current) {
				if (!stored) {
					editorRef.innerHTML = '<div><br></div>';
				} else if (stored.trim().startsWith('<')) {
					editorRef.innerHTML = stored;
				} else {
					editorRef.innerHTML = stored
						.split('\n')
						.map(line => line.trim() === '' ? '<br>' : `<div>${line}</div>`)
						.join('');
				}
				updateCounts();
			}
			lastSaved = new Date(activeNote.updatedAt).toLocaleTimeString();
		}
	});

	onDestroy(() => {
		if (saveTimeout) {
			clearTimeout(saveTimeout);
			doSave();
		}
		if (exportToastTimeout) clearTimeout(exportToastTimeout);
	});

	function exec(cmd: string, val?: string) {
		document.execCommand(cmd, false, val);
		updateContentFromEditor();
		editorRef?.focus();
	}

	function formatBold() { exec('bold'); }
	function formatItalic() { exec('italic'); }
	function formatUnderline() { exec('underline'); }

	function formatHighlight() {
		const selection = window.getSelection();
		if (!selection || selection.rangeCount === 0) return;
		const range = selection.getRangeAt(0);
		if (range.collapsed) return;
		const span = document.createElement('span');
		span.style.backgroundColor = '#fef08a';
		span.className = 'highlighted-text';
		try {
			range.surroundContents(span);
		} catch {
			exec('hiliteColor', '#fef08a');
			return;
		}
		selection.removeAllRanges();
		updateContentFromEditor();
		editorRef?.focus();
	}

	function formatBlock(tag: string) { exec('formatBlock', tag); }
	function formatBlockQuote() { exec('formatBlock', 'blockquote'); }

	function formatList(type: 'bullet' | 'dashed' | 'number') {
		if (type === 'number') {
			exec('insertOrderedList');
		} else {
			exec('insertUnorderedList');
			setTimeout(() => {
				const sel = window.getSelection();
				if (!sel) return;
				let node: Node | null = sel.anchorNode;
				let ul: HTMLUListElement | null = null;
				while (node) {
					if (node.nodeName === 'UL') { ul = node as HTMLUListElement; break; }
					node = node.parentNode;
				}
				if (ul) {
					if (type === 'dashed') {
						ul.style.listStyleType = 'none';
						ul.dataset.type = 'dashed';
					} else {
						ul.style.listStyleType = 'disc';
						delete ul.dataset.type;
					}
				}
				updateContentFromEditor();
			}, 10);
		}
	}

	function isInChecklist(): boolean {
		const sel = window.getSelection();
		if (!sel) return false;
		let node: Node | null = sel.anchorNode;
		while (node) {
			if (node.nodeName === 'LI' && (node as HTMLElement).parentElement?.classList.contains('checklist')) return true;
			node = node.parentNode;
		}
		return false;
	}

	function getCurrentChecklistItem(): HTMLLIElement | null {
		const sel = window.getSelection();
		if (!sel) return null;
		let node: Node | null = sel.anchorNode;
		while (node) {
			if (node.nodeName === 'LI') return node as HTMLLIElement;
			node = node.parentNode;
		}
		return null;
	}

	function toggleCheckList() {
		if (isInChecklist()) {
			const sel = window.getSelection();
			const li = getCurrentChecklistItem();
			if (li) {
				const ul = li.parentElement;
				const p = document.createElement('p');
				Array.from(li.childNodes).forEach(child => {
					if (!(child.nodeName === 'INPUT')) {
						p.appendChild(child.cloneNode(true));
					}
				});
				if (ul && ul.children.length === 1) {
					ul.replaceWith(p);
				} else {
					li.replaceWith(p);
				}
				const newRange = document.createRange();
				newRange.selectNodeContents(p);
				newRange.collapse(false);
				sel?.removeAllRanges();
				sel?.addRange(newRange);
			}
		} else {
			insertCheckList();
		}
		updateContentFromEditor();
		editorRef?.focus();
	}

	function insertCheckList() {
		if (!editorRef) return;
		const ul = document.createElement('ul');
		ul.className = 'checklist';
		ul.style.listStyle = 'none';
		ul.style.paddingLeft = '0';
		ul.appendChild(createCheckItem('Checklist item'));
		insertNodeAtCursor(ul);
		updateContentFromEditor();
		editorRef?.focus();
	}

	function createCheckItem(text: string): HTMLLIElement {
		const li = document.createElement('li');
		const cb = document.createElement('input');
		cb.type = 'checkbox';
		cb.style.marginTop = '0.3rem';
		cb.style.cursor = 'pointer';
		cb.style.marginRight = '0.5rem';
		cb.style.flexShrink = '0';
		cb.contentEditable = 'false';
		cb.onchange = () => {
			if (cb.checked) cb.setAttribute('checked', 'checked');
			else cb.removeAttribute('checked');
			updateContentFromEditor();
		};
		li.appendChild(cb);
		const span = document.createElement('span');
		span.textContent = text;
		li.appendChild(span);
		return li;
	}

	function insertTable() {
		if (!editorRef) return;
		const rows = Math.max(1, Math.min(20, Number(tableRows)));
		const cols = Math.max(1, Math.min(10, Number(tableCols)));
		const table = document.createElement('table');
		table.className = 'editor-table';
		table.style.width = '100%';
		table.style.borderCollapse = 'collapse';
		table.style.margin = '1rem 0';
		table.dataset.tableId = crypto.randomUUID();
		const tbody = document.createElement('tbody');
		for (let r = 0; r < rows; r++) {
			const tr = document.createElement('tr');
			for (let c = 0; c < cols; c++) {
				const td = document.createElement('td');
				td.style.border = '1px solid #e5e7eb';
				td.style.padding = '0.5rem';
				td.style.minWidth = '50px';
				td.innerHTML = '&nbsp;';
				tr.appendChild(td);
			}
			tbody.appendChild(tr);
		}
		table.appendChild(tbody);
		insertNodeAtCursor(table);
		showTableDialog = false;
		updateContentFromEditor();
		editorRef?.focus();
	}

	function deleteTable() {
		currentTable?.remove();
		currentTable = null;
		updateContentFromEditor();
	}

	function addTableRow(before: boolean) {
		if (!currentTable) return;
		const sel = window.getSelection();
		const td = sel?.anchorNode?.parentElement?.closest('td');
		const tr = td?.closest('tr');
		if (!tr) return;
		const newTr = tr.cloneNode(true) as HTMLTableRowElement;
		newTr.querySelectorAll('td').forEach(d => d.innerHTML = '&nbsp;');
		before ? tr.before(newTr) : tr.after(newTr);
		updateContentFromEditor();
	}

	function addTableCol(before: boolean) {
		if (!currentTable) return;
		const sel = window.getSelection();
		const td = sel?.anchorNode?.parentElement?.closest('td');
		if (!td) return;
		const idx = Array.from(td.parentElement?.children || []).indexOf(td);
		currentTable.querySelectorAll('tr').forEach(tr => {
			const cells = tr.querySelectorAll('td');
			const newTd = document.createElement('td');
			newTd.style.border = '1px solid #e5e7eb';
			newTd.style.padding = '0.5rem';
			newTd.innerHTML = '&nbsp;';
			if (cells[idx]) before ? cells[idx].before(newTd) : cells[idx].after(newTd);
			else tr.appendChild(newTd);
		});
		updateContentFromEditor();
	}

	function deleteTableRow() {
		const sel = window.getSelection();
		const td = sel?.anchorNode?.parentElement?.closest('td');
		const tr = td?.closest('tr');
		if (tr && currentTable && currentTable.querySelectorAll('tr').length > 1) {
			tr.remove();
			updateContentFromEditor();
		}
	}

	function deleteTableCol() {
		const sel = window.getSelection();
		const td = sel?.anchorNode?.parentElement?.closest('td');
		if (!td || !currentTable) return;
		const idx = Array.from(td.parentElement?.children || []).indexOf(td);
		currentTable.querySelectorAll('tr').forEach(tr => {
			const cells = tr.querySelectorAll('td');
			if (cells[idx] && cells.length > 1) cells[idx].remove();
		});
		updateContentFromEditor();
	}

	function checkTableSelection() {
		const sel = window.getSelection();
		if (!sel || !editorRef) { currentTable = null; return; }
		const td = sel.anchorNode?.parentElement?.closest('td');
		currentTable = td?.closest('table') ?? null;
	}

	function insertNodeAtCursor(node: Node) {
		const sel = window.getSelection();
		if (!sel || sel.rangeCount === 0 || !editorRef) {
			editorRef?.appendChild(node);
			return;
		}
		const range = sel.getRangeAt(0);
		range.deleteContents();
		range.insertNode(node);
		range.setStartAfter(node);
		range.setEndAfter(node);
		sel.removeAllRanges();
		sel.addRange(range);
	}

	function handleFileSelect(e: Event) {
		const target = e.target as HTMLInputElement;
		if (!target.files || !editorRef) return;
		Array.from(target.files).forEach(file => {
			const isText = file.type.startsWith('text/') || /\.(txt|md|json|js|ts|html|css|py|csv|xml|yaml|yml)$/i.test(file.name);
			if (isText) {
				const reader = new FileReader();
				reader.onload = ev => {
					const text = ev.target?.result as string;
					const wrapper = document.createElement('div');
					wrapper.style.margin = '0.5rem 0';
					wrapper.style.padding = '0.75rem';
					wrapper.style.background = 'hsl(var(--muted))';
					wrapper.style.borderRadius = '0.375rem';
					wrapper.style.fontFamily = 'ui-monospace, monospace';
					wrapper.style.fontSize = '0.8rem';
					wrapper.style.whiteSpace = 'pre-wrap';
					wrapper.style.wordBreak = 'break-word';
					wrapper.style.maxHeight = '300px';
					wrapper.style.overflow = 'auto';
					const header = document.createElement('div');
					header.style.fontWeight = '600';
					header.style.marginBottom = '0.5rem';
					header.style.fontSize = '0.75rem';
					header.style.color = 'hsl(var(--muted-foreground))';
					header.textContent = `📄 ${file.name} (${(file.size / 1024).toFixed(1)} KB)`;
					const pre = document.createElement('pre');
					pre.style.margin = '0';
					pre.style.background = 'transparent';
					pre.style.padding = '0';
					pre.textContent = text;
					wrapper.appendChild(header);
					wrapper.appendChild(pre);
					insertNodeAtCursor(wrapper);
					updateContentFromEditor();
				};
				reader.readAsText(file);
			} else if (file.type.startsWith('image/')) {
				const reader = new FileReader();
				reader.onload = ev => {
					const result = ev.target?.result as string;
					const img = document.createElement('img');
					img.src = result;
					img.style.maxWidth = '100%';
					img.style.borderRadius = '0.5rem';
					img.style.margin = '0.5rem 0';
					img.alt = file.name;
					insertNodeAtCursor(img);
					updateContentFromEditor();
				};
				reader.readAsDataURL(file);
			} else if (file.type.startsWith('video/')) {
				const reader = new FileReader();
				reader.onload = ev => {
					const result = ev.target?.result as string;
					const video = document.createElement('video');
					video.src = result;
					video.controls = true;
					video.style.maxWidth = '100%';
					video.style.borderRadius = '0.5rem';
					video.style.margin = '0.5rem 0';
					insertNodeAtCursor(video);
					updateContentFromEditor();
				};
				reader.readAsDataURL(file);
			} else {
				const reader = new FileReader();
				reader.onload = ev => {
					const result = ev.target?.result as string;
					const a = document.createElement('a');
					a.href = result;
					a.download = file.name;
					a.textContent = `📎 ${file.name}`;
					a.style.display = 'inline-flex';
					a.style.alignItems = 'center';
					a.style.gap = '0.5rem';
					a.style.padding = '0.5rem 1rem';
					a.style.background = 'hsl(var(--muted))';
					a.style.borderRadius = '0.375rem';
					a.style.margin = '0.25rem 0';
					a.style.textDecoration = 'none';
					a.style.color = 'inherit';
					a.style.fontSize = '0.875rem';
					insertNodeAtCursor(a);
					updateContentFromEditor();
				};
				reader.readAsDataURL(file);
			}
		});
		target.value = '';
	}

	function triggerFileUpload(type: 'image' | 'file') {
		if (type === 'file') fileInput?.click();
		else mediaInput?.click();
	}

	function initSketch(node: HTMLCanvasElement) {
		const ctx = node.getContext('2d');
		if (!ctx) return;
		sketchContext = ctx;
		ctx.lineCap = 'round';
		ctx.lineJoin = 'round';
		ctx.lineWidth = 3;
		ctx.strokeStyle = '#000';
	}

	function getSketchPos(e: MouseEvent | TouchEvent) {
		if (!sketchCanvas) return { x: 0, y: 0 };
		const rect = sketchCanvas.getBoundingClientRect();
		const cx = 'touches' in e ? e.touches[0].clientX : e.clientX;
		const cy = 'touches' in e ? e.touches[0].clientY : e.clientY;
		return { x: cx - rect.left, y: cy - rect.top };
	}

	function startSketch(e: MouseEvent | TouchEvent) {
		isDrawing = true;
		if (!sketchContext) return;
		const pos = getSketchPos(e);
		sketchContext.beginPath();
		sketchContext.moveTo(pos.x, pos.y);
	}

	function drawSketch(e: MouseEvent | TouchEvent) {
		if (!isDrawing || !sketchContext) return;
		const pos = getSketchPos(e);
		sketchContext.lineTo(pos.x, pos.y);
		sketchContext.stroke();
	}

	function endSketch() { isDrawing = false; sketchContext?.closePath(); }

	function saveSketch() {
		if (!sketchCanvas || !editorRef) return;
		const img = document.createElement('img');
		img.src = sketchCanvas.toDataURL('image/png');
		img.style.maxWidth = '100%';
		img.style.borderRadius = '0.5rem';
		img.style.margin = '0.5rem 0';
		img.alt = 'Sketch';
		insertNodeAtCursor(img);
		updateContentFromEditor();
		showSketchModal = false;
	}

	function clearSketch() {
		if (!sketchContext || !sketchCanvas) return;
		sketchContext.clearRect(0, 0, sketchCanvas.width, sketchCanvas.height);
	}

	function shareNote() {
		if (!activeNote) return;
		const text = `${activeNote.title}

${editorRef?.innerText || ''}`;
		if (navigator.share) {
			navigator.share({ title: activeNote.title, text }).catch(() => {});
		} else {
			navigator.clipboard.writeText(text).then(() => showToast('Note copied to clipboard!'));
		}
	}

	function showToast(msg: string) {
		exportToastMsg = msg;
		showExportToast = true;
		if (exportToastTimeout) clearTimeout(exportToastTimeout);
		exportToastTimeout = setTimeout(() => showExportToast = false, 3000);
	}

	function exportNote() {
		if (!activeNote) return;
		const blob = new Blob([`# ${activeNote.title}

${editorRef?.innerText || ''}`], { type: 'text/plain' });
		const url = URL.createObjectURL(blob);
		const a = document.createElement('a');
		a.href = url;
		const filename = `${activeNote.title || 'note'}.txt`;
		a.download = filename;
		a.click();
		URL.revokeObjectURL(url);
		showToast(`Downloaded: ${filename} → Check your Downloads folder`);
	}

	function printNote() {
		if (!editorRef || !activeNote) return;
		const w = window.open('', '_blank');
		if (!w) return;
		w.document.write(`<html><head><title>${activeNote.title}</title><style>body{font-family:system-ui;max-width:800px;margin:2rem auto;padding:1rem;line-height:1.6}</style></head><body><h1>${activeNote.title || 'Untitled'}</h1>${editorRef.innerHTML}</body></html>`);
		w.document.close();
		w.print();
	}

	function handleCreate() {
		notesStore.createNote();
		setTimeout(() => {
			const el = document.querySelector('.note-title-input') as HTMLInputElement | null;
			el?.focus();
		}, 50);
	}

	function handleDelete(id: string, e: Event) {
		e.stopPropagation();
		notesStore.deleteNote(id);
	}

	function selectNote(id: string) { notesStore.setActive(id); }

	function updateTitle(e: Event) {
		const target = e.target as HTMLInputElement;
		if (activeNote) notesStore.updateNote(activeNote.id, { title: target.value });
	}

	function setNoteColor(color: string | null) {
		const note = notesStore.activeNote;
		if (note) notesStore.updateNote(note.id, { color });
		showColorPicker = false;
	}

	function setNoteEmoji(emoji: string) {
		const note = notesStore.activeNote;
		if (note) notesStore.updateNote(note.id, { emoji: note.emoji === emoji ? null : emoji });
		showEmojiPicker = false;
	}

	function getNoteColorBorder(note: Note): string {
		return noteColors.find(c => c.value === note.color)?.border || '';
	}

	function handleEditorKeyDown(e: KeyboardEvent) {
		if ((e.ctrlKey || e.metaKey) && e.key === 's') {
			e.preventDefault();
			if (saveTimeout) clearTimeout(saveTimeout);
			doSave();
		}
		if (e.key === 'Enter' && !e.shiftKey) {
			const sel = window.getSelection();
			const li = getCurrentChecklistItem();
			if (li && li.parentElement?.classList.contains('checklist')) {
				e.preventDefault();
				const textContent = li.textContent?.replace(/^\s*/, '') || '';
				if (textContent.trim() === '') {
					const ul = li.parentElement;
					const p = document.createElement('p');
					p.innerHTML = '<br>';
					if (ul.children.length === 1) {
						ul.replaceWith(p);
					} else {
						li.remove();
						ul.after(p);
					}
					const range = document.createRange();
					range.selectNodeContents(p);
					range.collapse(true);
					sel?.removeAllRanges();
					sel?.addRange(range);
				} else {
					const newLi = createCheckItem('');
					li.after(newLi);
					const range = document.createRange();
					const span = newLi.querySelector('span');
					if (span) {
						range.selectNodeContents(span);
						range.collapse(true);
					} else {
						range.selectNodeContents(newLi);
						range.collapse(true);
					}
					sel?.removeAllRanges();
					sel?.addRange(range);
				}
				updateContentFromEditor();
				return;
			}
		}
		setTimeout(checkTableSelection, 0);
	}

	function handleClickOutside(e: MouseEvent) {
		const t = e.target as HTMLElement;
		if (!t.closest('.style-menu-container')) showStyleMenu = false;
		if (!t.closest('.more-menu-container')) showMoreMenu = false;
		if (!t.closest('.emoji-picker-container')) showEmojiPicker = false;
		if (!t.closest('.color-picker-container')) showColorPicker = false;
	}

	$effect(() => {
		document.addEventListener('click', handleClickOutside);
		return () => document.removeEventListener('click', handleClickOutside);
	});
</script>

<!-- ===== MAIN FLOATING WINDOW ===== -->
<FloatingWindow title="Notes" store={notesWindow}>
	<div class="flex h-full w-full overflow-hidden">
		<!-- Sidebar -->
		<div class="flex w-72 flex-col border-r bg-background">
			<div class="flex items-center justify-between border-b p-4">
				<h2 class="text-lg font-semibold flex items-center gap-2">
					<Sparkles class="h-5 w-5 text-yellow-500" />
					Notes
				</h2>
				<Button size="icon" variant="ghost" onclick={handleCreate} title="New note">
					<Plus class="h-4 w-4" />
				</Button>
			</div>
			<div class="p-3">
				<Input placeholder="Search notes..." bind:value={search} class="h-8" />
			</div>
			<ScrollArea class="flex-1">
				<div class="flex flex-col gap-1 p-2">
					{#each filteredNotes as note (note.id)}
						<div
							role="button"
							tabindex="0"
							class="group relative flex items-start gap-2 rounded-md px-3 py-2 text-left text-sm transition-all cursor-pointer border-l-4 border-transparent
								{notesStore.activeNoteId === note.id ? 'bg-accent text-accent-foreground ring-1 ring-accent' : 'hover:bg-muted'}
								{getNoteColorBorder(note)}"
							onclick={() => selectNote(note.id)}
							onkeydown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); selectNote(note.id); } }}
						>
							<div class="shrink-0 mt-0.5">
								{#if note.emoji}
									<span class="text-base">{note.emoji}</span>
								{:else}
									<FileText class="h-4 w-4 opacity-50" />
								{/if}
							</div>
							<div class="flex-1 overflow-hidden min-w-0">
								<div class="truncate font-medium flex items-center gap-1">
									{#if note.pinned}
										<Pin class="h-3 w-3 fill-current text-yellow-500 shrink-0" />
									{/if}
									{note.title || 'Untitled'}
								</div>
								<div class="truncate text-xs text-muted-foreground">
									{new Date(note.updatedAt).toLocaleDateString()} • {new Date(note.updatedAt).toLocaleTimeString([], {hour:'2-digit', minute:'2-digit'})}
								</div>
							</div>
							<button
								class="h-6 w-6 rounded opacity-0 transition-opacity hover:bg-destructive/10 group-hover:opacity-100 focus:opacity-100 flex items-center justify-center shrink-0"
								onclick={(e: Event) => handleDelete(note.id, e)}
								title="Delete note"
							>
								<Trash2 class="h-3 w-3 text-destructive" />
							</button>
						</div>
					{:else}
						<div class="px-3 py-8 text-center text-sm text-muted-foreground">
							<div class="mb-2 text-4xl">📝</div>
							<p>No notes yet. Click + to create one.</p>
						</div>
					{/each}
				</div>
			</ScrollArea>
		</div>

		<!-- Editor -->
		<div class="flex flex-1 flex-col min-w-0 bg-background relative">
			{#if activeNote}
				<!-- Title Bar -->
				<div class="border-b px-6 py-3 flex items-center gap-3 relative">
					<Input
						value={activeNote.title}
						oninput={updateTitle}
						class="note-title-input border-0 bg-transparent text-xl font-semibold shadow-none focus-visible:ring-0 px-0 flex-1"
						placeholder="Note title..."
					/>
					<div class="flex items-center gap-1">
						<Button size="icon" variant="ghost" onclick={() => showEmojiPicker = !showEmojiPicker} title="Emoji">
							<Smile class="h-4 w-4" />
						</Button>
						{#if showEmojiPicker}
							<div class="emoji-picker-container absolute right-4 top-14 z-50 bg-popover border rounded-lg shadow-lg p-2 grid grid-cols-6 gap-1 w-48">
								{#each emojis as emoji}
									<button class="hover:bg-accent rounded p-1 text-lg transition-colors" onclick={() => setNoteEmoji(emoji)}>{emoji}</button>
								{/each}
							</div>
						{/if}
						<Button size="icon" variant="ghost" onclick={() => showColorPicker = !showColorPicker} title="Color">
							<Palette class="h-4 w-4" />
						</Button>
						{#if showColorPicker}
							<div class="color-picker-container absolute right-4 top-14 z-50 bg-popover border rounded-lg shadow-lg p-2 flex flex-col gap-1 w-32">
								{#each noteColors as color}
									<button class="flex items-center gap-2 px-2 py-1 rounded hover:bg-accent text-xs transition-colors" onclick={() => setNoteColor(color.value)}>
										<div class="w-4 h-4 rounded-full border {color.value ? 'bg-' + color.value + '-400' : 'bg-background border-dashed'}"></div>
										{color.name}
									</button>
								{/each}
							</div>
						{/if}
						<Button size="icon" variant="ghost" onclick={() => notesStore.togglePin(activeNote.id)} title={activeNote.pinned ? 'Unpin' : 'Pin'}>
							<Pin class="h-4 w-4 {activeNote.pinned ? 'fill-current text-yellow-500' : ''}" />
						</Button>
					</div>
				</div>

				<!-- Beautiful Toolbar -->
				<div class="border-b px-4 py-2 flex items-center gap-1 flex-wrap bg-muted/20">
					<!-- Style Dropdown -->
					<div class="style-menu-container relative">
						<button
							class="inline-flex items-center gap-1.5 px-3 py-1.5 text-sm font-medium rounded-l-md border border-r-0 border-input bg-background hover:bg-accent hover:text-accent-foreground transition-colors"
							onclick={() => showStyleMenu = !showStyleMenu}
						>
							<Type class="h-3.5 w-3.5" />
							<span>Style</span>
							<ChevronDown class="h-3 w-3 opacity-60" />
						</button>
						{#if showStyleMenu}
							<div class="absolute left-0 top-full mt-1 z-50 w-56 bg-popover border rounded-lg shadow-xl p-1 overflow-hidden">
								<div class="grid grid-cols-2 gap-1 p-1 border-b">
									<button class="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-accent text-xs transition-colors" onclick={() => { formatBold(); showStyleMenu = false; }}>
										<Bold class="h-3 w-3" /> Bold
									</button>
									<button class="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-accent text-xs transition-colors" onclick={() => { formatItalic(); showStyleMenu = false; }}>
										<Italic class="h-3 w-3" /> Italic
									</button>
									<button class="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-accent text-xs transition-colors" onclick={() => { formatUnderline(); showStyleMenu = false; }}>
										<Underline class="h-3 w-3" /> Underline
									</button>
									<button class="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-accent text-xs transition-colors" onclick={() => { formatHighlight(); showStyleMenu = false; }}>
										<Highlighter class="h-3 w-3" /> Highlight
									</button>
								</div>
								<div class="flex flex-col p-1">
									<button class="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-accent text-xs transition-colors" onclick={() => { formatBlock('h1'); showStyleMenu = false; }}>
										<Heading1 class="h-3 w-3" /> Title (H1)
									</button>
									<button class="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-accent text-xs transition-colors" onclick={() => { formatBlock('h2'); showStyleMenu = false; }}>
										<Heading2 class="h-3 w-3" /> Heading (H2)
									</button>
									<button class="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-accent text-xs transition-colors" onclick={() => { formatBlock('h3'); showStyleMenu = false; }}>
										<Heading3 class="h-3 w-3" /> Subheading (H3)
									</button>
									<button class="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-accent text-xs transition-colors" onclick={() => { formatBlock('p'); showStyleMenu = false; }}>
										<Text class="h-3 w-3" /> Body (p)
									</button>
									<button class="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-accent text-xs transition-colors" onclick={() => { formatBlock('pre'); showStyleMenu = false; }}>
										<Code class="h-3 w-3" /> Monostyled (pre)
									</button>
								</div>
								<div class="border-t p-1 flex flex-col">
									<button class="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-accent text-xs transition-colors" onclick={() => { formatList('bullet'); showStyleMenu = false; }}>
										<List class="h-3 w-3" /> Bulleted List
									</button>
									<button class="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-accent text-xs transition-colors" onclick={() => { formatList('dashed'); showStyleMenu = false; }}>
										<Minus class="h-3 w-3" /> Dashed List
									</button>
									<button class="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-accent text-xs transition-colors" onclick={() => { formatList('number'); showStyleMenu = false; }}>
										<ListOrdered class="h-3 w-3" /> Number List
									</button>
									<button class="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-accent text-xs transition-colors" onclick={() => { formatBlockQuote(); showStyleMenu = false; }}>
										<Quote class="h-3 w-3" /> Block Quote
									</button>
								</div>
							</div>
						{/if}
					</div>

					<!-- Block Quote -->
					<button
						class="inline-flex items-center gap-1.5 px-3 py-1.5 text-sm font-medium border border-input bg-background hover:bg-accent hover:text-accent-foreground transition-colors"
						onclick={formatBlockQuote}
						title="Block Quote"
					>
						<Quote class="h-3.5 w-3.5" />
						<span class="hidden sm:inline">Block Quote</span>
					</button>

					<!-- Add Table -->
					<button
						class="inline-flex items-center gap-1.5 px-3 py-1.5 text-sm font-medium border border-input bg-background hover:bg-accent hover:text-accent-foreground transition-colors"
						onclick={() => { centerDialog(); showTableDialog = true; }}
						title="Add Table"
					>
						<Table class="h-3.5 w-3.5" />
						<span class="hidden sm:inline">Add Table</span>
					</button>

					<!-- Check List -->
					<button
						class="inline-flex items-center gap-1.5 px-3 py-1.5 text-sm font-medium border border-input bg-background hover:bg-accent hover:text-accent-foreground transition-colors {isInChecklist() ? 'bg-accent text-accent-foreground' : ''}"
						onclick={toggleCheckList}
						title="Toggle Check List"
					>
						<ListChecks class="h-3.5 w-3.5" />
						<span class="hidden sm:inline">Check List</span>
					</button>

					<!-- More >> -->
					<div class="more-menu-container relative">
						<button
							class="inline-flex items-center gap-1.5 px-3 py-1.5 text-sm font-medium rounded-r-md border border-l-0 border-input bg-background hover:bg-accent hover:text-accent-foreground transition-colors"
							onclick={() => showMoreMenu = !showMoreMenu}
							title="More options"
						>
							<MoreHorizontal class="h-3.5 w-3.5" />
							<span>&gt;&gt;</span>
						</button>
						{#if showMoreMenu}
							<div class="absolute right-0 top-full mt-1 z-50 w-56 bg-popover border rounded-lg shadow-xl p-1.5 overflow-hidden">
								<button class="w-full flex items-center gap-2.5 px-3 py-2 rounded-md hover:bg-accent text-sm transition-colors" onclick={() => { triggerFileUpload('image'); showMoreMenu = false; }}>
									<ImageIcon class="h-4 w-4 text-muted-foreground" /> Choose photo or video
								</button>
								<button class="w-full flex items-center gap-2.5 px-3 py-2 rounded-md hover:bg-accent text-sm transition-colors" onclick={() => { triggerFileUpload('file'); showMoreMenu = false; }}>
									<Paperclip class="h-4 w-4 text-muted-foreground" /> Attach file
								</button>
								<button class="w-full flex items-center gap-2.5 px-3 py-2 rounded-md hover:bg-accent text-sm transition-colors" onclick={() => { showSketchModal = true; showMoreMenu = false; }}>
									<Pencil class="h-4 w-4 text-muted-foreground" /> Add sketch
								</button>
								<div class="border-t my-1"></div>
								<button class="w-full flex items-center gap-2.5 px-3 py-2 rounded-md hover:bg-accent text-sm transition-colors" onclick={() => { shareNote(); showMoreMenu = false; }}>
									<Share2 class="h-4 w-4 text-muted-foreground" /> Share
								</button>
								<button class="w-full flex items-center gap-2.5 px-3 py-2 rounded-md hover:bg-accent text-sm transition-colors" onclick={() => { exportNote(); showMoreMenu = false; }}>
									<Download class="h-4 w-4 text-muted-foreground" /> Export
								</button>
								<button class="w-full flex items-center gap-2.5 px-3 py-2 rounded-md hover:bg-accent text-sm transition-colors" onclick={() => { printNote(); showMoreMenu = false; }}>
									<Printer class="h-4 w-4 text-muted-foreground" /> Print
								</button>
							</div>
						{/if}
					</div>
				</div>

				<!-- Editor Content -->
				<div class="flex-1 relative overflow-hidden">
					<div
						bind:this={editorRef}
						use:setupEditor
						contenteditable="true"
						class="h-full w-full overflow-y-auto p-6 outline-none text-base leading-relaxed"
						style="min-height: 200px;"
						onkeydown={handleEditorKeyDown}
						onmouseup={() => checkTableSelection()}
						onblur={() => { currentTable = null; if (saveTimeout) { clearTimeout(saveTimeout); doSave(); } }}
					></div>

					{#if currentTable}
						<div class="absolute top-2 left-1/2 -translate-x-1/2 flex items-center gap-0.5 bg-popover border rounded-md shadow-xl p-1 z-40">
							<Button size="sm" variant="ghost" class="h-7 gap-1 text-xs px-2" onclick={() => addTableRow(true)}><ArrowUp class="h-3 w-3" /> Row</Button>
							<Button size="sm" variant="ghost" class="h-7 gap-1 text-xs px-2" onclick={() => addTableRow(false)}><ArrowDown class="h-3 w-3" /> Row</Button>
							<div class="w-px h-4 bg-border mx-0.5"></div>
							<Button size="sm" variant="ghost" class="h-7 gap-1 text-xs px-2" onclick={() => addTableCol(true)}><ArrowLeft class="h-3 w-3" /> Col</Button>
							<Button size="sm" variant="ghost" class="h-7 gap-1 text-xs px-2" onclick={() => addTableCol(false)}><ArrowRight class="h-3 w-3" /> Col</Button>
							<div class="w-px h-4 bg-border mx-0.5"></div>
							<Button size="sm" variant="ghost" class="h-7 text-xs text-destructive px-2" onclick={deleteTableRow}>Del Row</Button>
							<Button size="sm" variant="ghost" class="h-7 text-xs text-destructive px-2" onclick={deleteTableCol}>Del Col</Button>
							<Button size="sm" variant="ghost" class="h-7 gap-1 text-xs text-destructive px-2" onclick={deleteTable}><Trash class="h-3 w-3" /> Table</Button>
						</div>
					{/if}
				</div>

				<!-- Status Bar -->
				<div class="border-t px-4 py-1.5 flex items-center justify-between text-xs text-muted-foreground bg-muted/20">
					<div class="flex items-center gap-3">
						<span>{wordCount} words</span>
						<span>{charCount} chars</span>
						{#if wordCount > 0}
							<span>~{Math.ceil(wordCount / 200)} min read</span>
						{/if}
					</div>
					<div class="flex items-center gap-2">
						{#if lastSaved}<span>Saved at {lastSaved}</span>{/if}
					</div>
				</div>
			{:else}
				<div class="flex flex-1 items-center justify-center text-muted-foreground">
					<div class="text-center">
						<FileText class="mx-auto mb-3 h-10 w-10 opacity-20" />
						<p>Select a note or create a new one</p>
						<Button class="mt-4" onclick={handleCreate}>
							<Plus class="mr-2 h-4 w-4" /> New Note
						</Button>
					</div>
				</div>
			{/if}
		</div>
	</div>
</FloatingWindow>

<!-- ===== HIDDEN FILE INPUTS ===== -->
<input type="file" bind:this={fileInput} class="hidden" onchange={handleFileSelect} accept="*/*" multiple />
<input type="file" bind:this={mediaInput} class="hidden" onchange={handleFileSelect} accept="image/*,video/*" multiple />

<!-- ===== TABLE DIALOG — OUTSIDE FloatingWindow (not clipped by overflow:hidden) ===== -->
{#if showTableDialog}
	<div
		class="fixed inset-0 bg-black/40"
		style="z-index: 100000;"
		onclick={() => showTableDialog = false}
	></div>
	<div
		class="fixed bg-background border rounded-lg shadow-2xl p-6 w-80"
		style="left: {tableDlgX}px; top: {tableDlgY}px; z-index: 100001;"
	>
		<div
			class="flex items-center justify-between mb-4 cursor-grab active:cursor-grabbing select-none"
			onpointerdown={onTableDragStart}
			onpointermove={onTableDragMove}
			onpointerup={onTableDragEnd}
			onpointercancel={onTableDragEnd}
		>
			<h3 class="text-lg font-semibold flex items-center gap-2">
				<GripVertical class="h-4 w-4 text-muted-foreground" />
				Insert Table
			</h3>
			<Button size="icon" variant="ghost" onclick={() => showTableDialog = false}>
				<X class="h-4 w-4" />
			</Button>
		</div>
		<div class="space-y-4">
			<div>
				<label class="text-sm text-muted-foreground block mb-1">Rows</label>
				<Input type="number" bind:value={tableRows} min={1} max={20} />
			</div>
			<div>
				<label class="text-sm text-muted-foreground block mb-1">Columns</label>
				<Input type="number" bind:value={tableCols} min={1} max={10} />
			</div>
		</div>
		<div class="flex justify-end gap-2 mt-6">
			<Button variant="ghost" onclick={() => showTableDialog = false}>Cancel</Button>
			<Button onclick={insertTable}>Insert</Button>
		</div>
	</div>
{/if}

<!-- ===== SKETCH MODAL — OUTSIDE FloatingWindow ===== -->
{#if showSketchModal}
	<div
		class="fixed inset-0 bg-black/40"
		style="z-index: 100000;"
		onclick={() => showSketchModal = false}
	></div>
	<div
		class="fixed bg-background border rounded-lg shadow-2xl p-4 max-w-[90vw]"
		style="left: {sketchDlgX}px; top: {sketchDlgY}px; z-index: 100001; width: 600px;"
	>
		<div
			class="flex items-center justify-between mb-4 cursor-grab active:cursor-grabbing select-none"
			onpointerdown={onSketchDragStart}
			onpointermove={onSketchDragMove}
			onpointerup={onSketchDragEnd}
			onpointercancel={onSketchDragEnd}
		>
			<h3 class="text-lg font-semibold flex items-center gap-2">
				<GripVertical class="h-4 w-4 text-muted-foreground" />
				Sketch
			</h3>
			<Button size="icon" variant="ghost" onclick={() => showSketchModal = false}>
				<X class="h-4 w-4" />
			</Button>
		</div>
		<canvas
			bind:this={sketchCanvas}
			use:initSketch
			width={560}
			height={300}
			class="border rounded bg-white cursor-crosshair w-full touch-none block"
			onmousedown={startSketch}
			onmousemove={drawSketch}
			onmouseup={endSketch}
			onmouseleave={endSketch}
			onclick={(e) => e.stopPropagation()}
		></canvas>
		<div class="flex justify-end gap-2 mt-4">
			<Button variant="ghost" onclick={clearSketch}>Clear</Button>
			<Button variant="ghost" onclick={() => showSketchModal = false}>Cancel</Button>
			<Button onclick={saveSketch}>Save Sketch</Button>
		</div>
	</div>
{/if}

<!-- ===== EXPORT TOAST — OUTSIDE FloatingWindow ===== -->
{#if showExportToast}
	<div
		class="fixed bottom-6 right-6 bg-popover border rounded-lg shadow-xl px-4 py-3 flex items-center gap-2"
		style="z-index: 100001;"
	>
		<Download class="h-4 w-4 text-green-500" />
		<span class="text-sm font-medium">{exportToastMsg}</span>
	</div>
{/if}

<style>
	:global(.editor-table) {
		width: 100%;
		border-collapse: collapse;
		margin: 1rem 0;
	}
	:global(.editor-table td) {
		border: 1px solid hsl(var(--border));
		padding: 0.5rem;
		min-width: 50px;
		vertical-align: top;
	}
	:global(.editor-table td:focus) {
		outline: 2px solid hsl(var(--ring));
		outline-offset: -2px;
	}
	:global(blockquote) {
		border-left: 3px solid hsl(var(--border));
		padding-left: 1rem;
		margin: 1rem 0;
		font-style: italic;
		color: hsl(var(--muted-foreground));
	}
	:global(pre) {
		background: hsl(var(--muted));
		padding: 0.75rem;
		border-radius: 0.375rem;
		font-family: ui-monospace, monospace;
		font-size: 0.875rem;
		overflow-x: auto;
	}
	:global(.highlighted-text) {
		background-color: #fef08a;
		padding: 0 2px;
		border-radius: 2px;
	}
	:global(ul[data-type="dashed"]) {
		list-style: none;
		padding-left: 1.5rem;
	}
	:global(ul[data-type="dashed"] li::before) {
		content: "-";
		display: inline-block;
		width: 1em;
		margin-left: -1em;
	}
	:global(.checklist) {
		list-style: none;
		padding-left: 0;
	}
	:global(.checklist li) {
		display: flex;
		align-items: flex-start;
		gap: 0.5rem;
		margin-bottom: 0.25rem;
	}
	:global(.checklist input[type="checkbox"]) {
		margin-top: 0.25rem;
		cursor: pointer;
		flex-shrink: 0;
	}
</style>
