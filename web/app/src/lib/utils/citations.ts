/**
 * Citations in answers about attached files: finding them in the text, the file they point at,
 * and the words of the cited passage, so the viewer can open the file there and highlight it.
 */

/** A place in an attached file: what a citation or a reported source points at. */
export interface CitationTarget {
	/** File name as cited (or as the engine reported it). */
	name: string;
	docId?: string;
	pageStart?: number;
	pageEnd?: number;
	sectionStart?: number;
	sectionEnd?: number;
	/** Text of the passage, to highlight where it sits. */
	passage?: string;
}

/** Where a file's content can be found on this device. */
export interface AttachedDocument {
	name: string;
	mimeType?: string;
	/** Original bytes (PDFs), base64. */
	base64Data?: string;
	/** Engine id, when the file was indexed; its text comes from the engine. */
	docId?: string;
	/** The whole text, for files that were sent with the message. */
	content?: string;
}

// [report.pdf, p.12] [report.pdf, p.12-13] [report.pdf, pp. 3–4] [report.pdf, pages 3-4]
// [notes.md, section 4] [notes.md, sections 4-5]
const CITATION =
	/\[([^[\]\n]{1,200}?),\s*(?:(?:pp?\.|pages?)\s*(\d+)(?:\s*[-–]\s*(\d+))?|sections?\s+(\d+)(?:\s*[-–]\s*(\d+))?)\s*\]/gi;

const toInt = (v?: string) => (v ? Number.parseInt(v, 10) : undefined);

/** Splits text into plain runs and citations, in order. */
export function splitCitations(text: string): Array<string | CitationTarget> {
	const out: Array<string | CitationTarget> = [];
	let last = 0;
	for (const m of text.matchAll(CITATION)) {
		const at = m.index ?? 0;
		if (at > last) out.push(text.slice(last, at));
		const pageStart = toInt(m[2]);
		const sectionStart = toInt(m[4]);
		out.push({
			name: m[1].trim(),
			pageStart,
			pageEnd: pageStart !== undefined ? (toInt(m[3]) ?? pageStart) : undefined,
			sectionStart,
			sectionEnd: sectionStart !== undefined ? (toInt(m[5]) ?? sectionStart) : undefined
		});
		last = at + m[0].length;
	}
	if (last < text.length) out.push(text.slice(last));
	return out;
}

/** "p.41–42", "p.7", "section 4" or "". */
export function citationLabel(t: CitationTarget): string {
	const range = (a?: number, b?: number) => (a === undefined ? '' : b && b > a ? `${a}–${b}` : `${a}`);
	if (t.pageStart !== undefined) return `p.${range(t.pageStart, t.pageEnd)}`;
	if (t.sectionStart !== undefined) return `section ${range(t.sectionStart, t.sectionEnd)}`;
	return '';
}

export function sourceToTarget(s: AgentSource): CitationTarget {
	return {
		name: s.document,
		docId: s.document_id,
		pageStart: s.page_start,
		pageEnd: s.page_end,
		sectionStart: s.section_start,
		sectionEnd: s.section_end,
		passage: s.text
	};
}

const sameName = (a: string, b: string) => a.trim().toLowerCase() === b.trim().toLowerCase();

/**
 * A cited place, completed from the answer's reported sources: an inline citation names only the
 * file and page, the source also knows the document id and the passage to highlight.
 */
export function resolveCitation(t: CitationTarget, sources: AgentSource[] | undefined): CitationTarget {
	const candidates = (sources ?? []).filter((s) => sameName(s.document, t.name));
	const overlaps = (a?: number, b?: number, c?: number, d?: number) =>
		a !== undefined && c !== undefined && a <= (d ?? c) && c <= (b ?? a);
	const best =
		candidates.find(
			(s) =>
				overlaps(t.pageStart, t.pageEnd, s.page_start, s.page_end) ||
				overlaps(t.sectionStart, t.sectionEnd, s.section_start, s.section_end)
		) ?? candidates[0];
	if (!best) return t;
	const sameSpot =
		overlaps(t.pageStart, t.pageEnd, best.page_start, best.page_end) ||
		overlaps(t.sectionStart, t.sectionEnd, best.section_start, best.section_end);
	return { ...t, docId: t.docId ?? best.document_id, passage: t.passage ?? (sameSpot ? best.text : undefined) };
}

/** Finds a cited file among the conversation's attachments, newest message first. */
export function findAttachedDocument(
	messages: DatabaseMessage[],
	target: Pick<CitationTarget, 'name' | 'docId'>
): AttachedDocument | null {
	for (let i = messages.length - 1; i >= 0; i--) {
		for (const extra of messages[i].extra ?? []) {
			if (extra.type === 'documentFile') {
				const hit = (target.docId && extra.docId === target.docId) || sameName(extra.name, target.name);
				if (hit)
					return {
						name: extra.name,
						mimeType: extra.mimeType,
						base64Data: extra.base64Data,
						docId: extra.docId,
						content: extra.content
					};
			} else if (extra.type === 'pdfFile' && sameName(extra.name, target.name)) {
				return {
					name: extra.name,
					mimeType: 'application/pdf',
					base64Data: extra.base64Data,
					content: extra.content
				};
			} else if (extra.type === 'textFile' && sameName(extra.name, target.name)) {
				return { name: extra.name, content: extra.content };
			}
		}
	}
	// Not in this chat (a file kept for all chats): the engine still has its text.
	return target.docId ? { name: target.name, docId: target.docId } : null;
}

/** One section of a document's text, as the viewer shows it. */
export interface DocumentSection {
	section: number;
	pageStart?: number;
	pageEnd?: number;
	heading?: string;
	text: string;
}

/** Splits text sent with a message back into its pages, using the `[Page N]` markers. */
export function sectionsFromContent(content: string): DocumentSection[] {
	const parts = content.split(/^\[Page (\d+)\]\s*$/m);
	if (parts.length === 1) return [{ section: 1, text: content }];
	const out: DocumentSection[] = [];
	if (parts[0].trim()) out.push({ section: 1, text: parts[0].trim() });
	for (let i = 1; i < parts.length; i += 2) {
		const page = Number.parseInt(parts[i], 10);
		out.push({ section: out.length + 1, pageStart: page, pageEnd: page, text: parts[i + 1].trim() });
	}
	return out;
}

/** The section a cited place falls in, or -1. */
export function sectionIndexFor(sections: DocumentSection[], t: CitationTarget): number {
	if (t.pageStart !== undefined) {
		const i = sections.findIndex(
			(s) => s.pageStart !== undefined && s.pageStart <= t.pageStart! && t.pageStart! <= (s.pageEnd ?? s.pageStart)
		);
		if (i >= 0) return i;
	}
	if (t.sectionStart !== undefined) {
		const i = sections.findIndex((s) => s.section === t.sectionStart);
		if (i >= 0) return i;
	}
	if (t.passage) {
		const i = sections.findIndex((s) => highlightRanges(s.text, t.passage!).length > 0);
		if (i >= 0) return i;
	}
	return -1;
}

const WORD = /[\p{L}\p{N}]+/gu;

/** The words of a text with where each sits, lowercased for matching. */
export function words(text: string): Array<{ word: string; start: number; end: number }> {
	return [...text.matchAll(WORD)].map((m) => ({
		word: m[0].toLowerCase(),
		start: m.index ?? 0,
		end: (m.index ?? 0) + m[0].length
	}));
}

/**
 * Which of `pageWords` belong to the passage: runs of at least `n` words (fewer for a short
 * passage) that appear in the same order in it. Tolerant of the line breaks, hyphenation and
 * spacing that make an exact text search fail on extracted PDF text.
 */
export function matchPassage(pageWords: string[], passage: string, n = 4): boolean[] {
	const p = words(passage).map((w) => w.word);
	const size = Math.max(1, Math.min(n, p.length));
	const grams = new Set<string>();
	for (let i = 0; i + size <= p.length; i++) grams.add(p.slice(i, i + size).join(' '));
	const marked = new Array<boolean>(pageWords.length).fill(false);
	if (p.length === 0) return marked;
	for (let i = 0; i + size <= pageWords.length; i++) {
		if (grams.has(pageWords.slice(i, i + size).join(' '))) {
			for (let k = i; k < i + size; k++) marked[k] = true;
		}
	}
	return marked;
}

/** Character ranges of `text` that belong to the passage, merged across spaces and punctuation. */
export function highlightRanges(text: string, passage: string): Array<[number, number]> {
	const w = words(text);
	const marked = matchPassage(
		w.map((x) => x.word),
		passage
	);
	const ranges: Array<[number, number]> = [];
	w.forEach((x, i) => {
		if (!marked[i]) return;
		const last = ranges[ranges.length - 1];
		if (last && x.start - last[1] <= 3) last[1] = x.end;
		else ranges.push([x.start, x.end]);
	});
	return ranges;
}
