<script lang="ts">
	import * as Dialog from '$lib/components/ui/dialog';
	import { Button } from '$lib/components/ui/button';
	import { ChevronLeft, ChevronRight, LocateFixed } from '@lucide/svelte';
	import { activeConversation, activeMessages } from '$lib/stores/chat.svelte';
	import { documentsService } from '$lib/services/documents';
	import {
		bestPageFor,
		citationLabel,
		findAttachedDocument,
		highlightRanges,
		matchPassage,
		sectionIndexFor,
		sectionsFromContent,
		words,
		type AttachedDocument,
		type CitationTarget,
		type DocumentSection
	} from '$lib/utils/citations';
	import {
		openPDF,
		pageSize,
		pageText,
		renderPDFPage,
		type OpenedPDF,
		type PageTextBox
	} from '$lib/utils/pdf-processing';
	import { onDestroy, tick } from 'svelte';

	interface Props {
		open: boolean;
		/** The page or section to open at. */
		place: CitationTarget | null;
		/** The file itself, when the caller already has it; otherwise found in the open chat. */
		document?: AttachedDocument | null;
	}

	let { open = $bindable(), place: target, document: given = null }: Props = $props();

	const SCALE = 1.3;
	let mode = $state<'loading' | 'pdf' | 'text' | 'missing'>('loading');
	let message = $state('');
	let pdf: OpenedPDF | null = null;
	let pageSizes = $state<Array<{ width: number; height: number }>>([]);
	let currentPage = $state(1);
	let highlights = $state<Record<number, PageTextBox[]>>({});
	let sections = $state<DocumentSection[]>([]);
	let activeSection = $state(-1);
	let scroller = $state<HTMLDivElement>();
	let canvases = $state<HTMLCanvasElement[]>([]);
	let pageElements = $state<HTMLElement[]>([]);
	let sectionElements = $state<HTMLElement[]>([]);
	let rendered = new Set<number>();
	let observer: IntersectionObserver | null = null;
	let loadedFor: CitationTarget | null = null;

	const name = $derived(target?.name ?? '');
	const where = $derived(target ? citationLabel(target) : '');
	let citedPage = $state(1);
	const isPdf = (doc: AttachedDocument) =>
		!!doc.base64Data && (doc.mimeType === 'application/pdf' || /\.pdf$/i.test(doc.name));

	$effect(() => {
		if (open && target && target !== loadedFor) {
			loadedFor = target;
			void load(target);
		}
		if (!open) loadedFor = null;
	});

	onDestroy(() => {
		observer?.disconnect();
		void pdf?.loadingTask.destroy();
	});

	async function load(t: CitationTarget) {
		mode = 'loading';
		highlights = {};
		rendered = new Set();
		observer?.disconnect();
		const doc = given ?? findAttachedDocument(activeMessages(), t);
		try {
			if (doc && isPdf(doc)) {
				await loadPdf(doc, t);
			} else if (doc?.docId) {
				sections = await documentsService.sections(doc.docId, activeConversation()?.id);
				await showText(t);
			} else if (doc?.content) {
				sections = sectionsFromContent(doc.content);
				await showText(t);
			} else {
				mode = 'missing';
				message = `"${t.name}" is not stored on this device, so it cannot be opened here.`;
			}
		} catch (error) {
			console.error('Could not open the cited file:', error);
			mode = 'missing';
			message = `"${t.name}" could not be opened.`;
		}
	}

	async function loadPdf(doc: AttachedDocument, t: CitationTarget) {
		void pdf?.loadingTask.destroy();
		pdf = await openPDF(doc.base64Data!);
		// Every page is laid out at the first page's size, so the scroll position is right before
		// the pages themselves are drawn.
		const first = await pageSize(pdf, 1, SCALE);
		pageSizes = Array.from({ length: pdf.numPages }, () => first);
		mode = 'pdf';
		await tick();
		observer = new IntersectionObserver(
			(entries) => {
				for (const e of entries) {
					const n = Number((e.target as HTMLElement).dataset.page);
					if (e.isIntersecting) void renderPage(n);
					if (e.isIntersecting && e.intersectionRatio > 0.5) currentPage = n;
				}
			},
			{ root: scroller, threshold: [0, 0.5] }
		);
		pageElements.forEach((el) => el && observer!.observe(el));
		citedPage = await pageHoldingPassage(t);
		await goToPage(citedPage);
	}

	// A source can span pages; open at the one that holds the passage (reading up to 40 pages).
	async function pageHoldingPassage(t: CitationTarget): Promise<number> {
		const first = Math.min(Math.max(1, t.pageStart ?? 1), pdf!.numPages);
		const last = Math.min(t.pageEnd ?? first, pdf!.numPages, first + 39);
		if (!t.passage || last <= first) return first;
		const texts: string[] = [];
		for (let n = first; n <= last; n++) texts.push(await pageText(pdf!, n));
		return first + bestPageFor(texts, t.passage);
	}

	async function renderPage(n: number) {
		if (!pdf || rendered.has(n) || !canvases[n - 1]) return;
		rendered.add(n);
		const boxes = await renderPDFPage(pdf, n, canvases[n - 1], SCALE);
		const t = target;
		const cited = t && n >= (t.pageStart ?? 0) && n <= (t.pageEnd ?? t.pageStart ?? 0);
		if (cited && t?.passage) {
			// Mark every piece of text holding a word of the passage.
			const owner: number[] = [];
			const pageWords: string[] = [];
			boxes.forEach((box, i) =>
				words(box.str).forEach((w) => {
					pageWords.push(w.word);
					owner.push(i);
				})
			);
			const marked = matchPassage(pageWords, t.passage);
			const hit = new Set(owner.filter((_, k) => marked[k]));
			highlights = { ...highlights, [n]: boxes.filter((_, i) => hit.has(i)) };
		}
	}

	async function goToPage(n: number) {
		if (!pdf) return;
		currentPage = Math.min(Math.max(1, n), pdf.numPages);
		await renderPage(currentPage);
		await tick();
		const el = pageElements[currentPage - 1];
		const mark = highlights[currentPage]?.[0];
		if (el && scroller) {
			// Center the highlighted passage when there is one, else show the top of the page.
			const offset = mark ? Math.max(0, mark.top - scroller.clientHeight / 3) : 0;
			scroller.scrollTop = el.offsetTop - scroller.offsetTop + offset;
		}
	}

	async function showText(t: CitationTarget) {
		mode = 'text';
		activeSection = sectionIndexFor(sections, t);
		await tick();
		showActiveSection();
	}

	function showActiveSection() {
		const el = sectionElements[activeSection];
		const mark = el?.querySelector('mark') as HTMLElement | null;
		(mark ?? el)?.scrollIntoView({ block: 'center' });
	}

	/** The text of a section split into plain and highlighted runs. */
	function runs(text: string, highlight: boolean): Array<{ text: string; marked: boolean }> {
		if (!highlight || !target?.passage) return [{ text, marked: false }];
		const out: Array<{ text: string; marked: boolean }> = [];
		let last = 0;
		for (const [a, b] of highlightRanges(text, target.passage)) {
			if (a > last) out.push({ text: text.slice(last, a), marked: false });
			out.push({ text: text.slice(a, b), marked: true });
			last = b;
		}
		if (last < text.length) out.push({ text: text.slice(last), marked: false });
		return out;
	}

	function sectionLabel(s: DocumentSection): string {
		if (s.pageStart !== undefined)
			return s.pageEnd && s.pageEnd > s.pageStart ? `Pages ${s.pageStart}–${s.pageEnd}` : `Page ${s.pageStart}`;
		return s.heading ?? `Section ${s.section}`;
	}
</script>

<Dialog.Root bind:open>
	<Dialog.Content class="flex h-[90vh] max-w-4xl flex-col gap-3 overflow-hidden sm:max-w-4xl">
		<Dialog.Header class="shrink-0">
			<Dialog.Title class="truncate pr-8">{name}</Dialog.Title>
			<Dialog.Description class="flex flex-wrap items-center gap-2 text-xs">
				{#if where}<span>Cited at {where}</span>{/if}
				{#if mode === 'pdf'}
					<span aria-live="polite">· Page {currentPage} of {pageSizes.length}</span>
				{/if}
			</Dialog.Description>
		</Dialog.Header>

		{#if mode === 'pdf' || mode === 'text'}
			<div class="flex shrink-0 flex-wrap items-center gap-2">
				{#if mode === 'pdf'}
					<Button
						variant="outline"
						size="sm"
						aria-label="Previous page"
						disabled={currentPage <= 1}
						onclick={() => goToPage(currentPage - 1)}
					>
						<ChevronLeft class="h-4 w-4" />
					</Button>
					<Button
						variant="outline"
						size="sm"
						aria-label="Next page"
						disabled={currentPage >= pageSizes.length}
						onclick={() => goToPage(currentPage + 1)}
					>
						<ChevronRight class="h-4 w-4" />
					</Button>
				{/if}
				<Button
					variant="outline"
					size="sm"
					onclick={() => (mode === 'pdf' ? goToPage(citedPage) : showActiveSection())}
				>
					<LocateFixed class="mr-1 h-4 w-4" /> Show cited passage
				</Button>
			</div>
		{/if}

		<div bind:this={scroller} class="relative min-h-0 flex-1 overflow-auto rounded-md border bg-muted/30">
			{#if mode === 'loading'}
				<p class="p-6 text-sm text-muted-foreground">Opening…</p>
			{:else if mode === 'missing'}
				<p role="alert" class="p-6 text-sm text-muted-foreground">{message}</p>
			{:else if mode === 'pdf'}
				<div class="flex flex-col items-center gap-4 p-4">
					{#each pageSizes as size, i (i)}
						<div
							bind:this={pageElements[i]}
							data-page={i + 1}
							aria-label="Page {i + 1}"
							class="relative bg-white shadow-sm"
							style="width: {size.width}px; height: {size.height}px;"
						>
							<canvas bind:this={canvases[i]} class="block"></canvas>
							{#each highlights[i + 1] ?? [] as box, k (k)}
								<span
									data-highlight
									class="pointer-events-none absolute rounded-sm bg-yellow-300/50 mix-blend-multiply"
									style="left: {box.left}px; top: {box.top}px; width: {box.width}px; height: {box.height}px;"
								></span>
							{/each}
						</div>
					{/each}
				</div>
			{:else}
				<div class="space-y-6 p-6 text-sm leading-relaxed">
					{#each sections as s, i (s.section)}
						<section
							bind:this={sectionElements[i]}
							aria-current={i === activeSection ? 'location' : undefined}
							class="rounded-md p-3 {i === activeSection ? 'bg-primary/5 ring-1 ring-primary/30' : ''}"
						>
							<h3 class="mb-2 text-xs font-medium text-muted-foreground">{sectionLabel(s)}</h3>
							<p class="whitespace-pre-wrap">
								{#each runs(s.text, i === activeSection) as run, k (k)}
									{#if run.marked}<mark class="rounded-sm bg-yellow-300/60 px-0.5">{run.text}</mark
										>{:else}{run.text}{/if}
								{/each}
							</p>
						</section>
					{/each}
				</div>
			{/if}
		</div>
	</Dialog.Content>
</Dialog.Root>
