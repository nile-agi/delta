/**
 * Turns an attached document into text segments the engine can index: one per PDF page, one per
 * spreadsheet sheet, or one for a whole Word or text file. The Word and spreadsheet parsers are
 * imported on demand, so they cost nothing until someone attaches such a file.
 */

import { FileExtensionDocument, MimeTypeDocument } from '$lib/enums/files';

export interface DocumentSegment {
	/** 1-based page number, for files that have pages. */
	page?: number;
	/** Sheet name, for spreadsheets. */
	label?: string;
	text: string;
	/** Every line is a self-contained record (a spreadsheet row). */
	records?: boolean;
}

export type DocumentKind = 'docx' | 'sheet' | 'csv';

function extensionOf(name: string): string {
	const dot = name.lastIndexOf('.');
	return dot < 0 ? '' : name.slice(dot).toLowerCase();
}

/** Which parser a file needs, judged by type and, since browsers often leave it blank, by name. */
export function documentKind(name: string, mimeType: string): DocumentKind | null {
	const ext = extensionOf(name);
	if (mimeType === MimeTypeDocument.DOCX || ext === FileExtensionDocument.DOCX) return 'docx';
	if (
		mimeType === MimeTypeDocument.XLSX ||
		mimeType === MimeTypeDocument.XLS ||
		mimeType === MimeTypeDocument.ODS ||
		ext === FileExtensionDocument.XLSX ||
		ext === FileExtensionDocument.XLS ||
		ext === FileExtensionDocument.ODS
	)
		return 'sheet';
	if (mimeType === 'text/csv' || ext === '.csv' || ext === '.tsv') return 'csv';
	return null;
}

const ENTITIES: Record<string, string> = {
	amp: '&',
	lt: '<',
	gt: '>',
	quot: '"',
	apos: "'",
	nbsp: ' '
};

function decodeEntities(text: string): string {
	return text.replace(/&(#x[0-9a-f]+|#\d+|[a-z]+);/gi, (match, code: string) => {
		if (code[0] === '#') {
			const n = code[1] === 'x' || code[1] === 'X' ? parseInt(code.slice(2), 16) : parseInt(code.slice(1));
			return Number.isFinite(n) ? String.fromCodePoint(n) : match;
		}
		return ENTITIES[code.toLowerCase()] ?? match;
	});
}

function inlineText(html: string): string {
	return decodeEntities(
		html
			.replace(/<br\s*\/?>/gi, '\n')
			.replace(/<[^>]+>/g, '')
			.replace(/[ \t]+/g, ' ')
	).trim();
}

/**
 * Mammoth's HTML as plain text that keeps the structure retrieval needs: headings as `#` lines,
 * list items as `- ` lines, table rows as `cell | cell`, and a blank line between blocks.
 */
export function docxHtmlToText(html: string): string {
	const blocks: string[] = [];
	const pattern = /<(h[1-6]|p|li|tr)\b[^>]*>([\s\S]*?)<\/\1>/gi;
	let match: RegExpExecArray | null;
	while ((match = pattern.exec(html)) !== null) {
		const tag = match[1].toLowerCase();
		const body = match[2];
		if (tag === 'tr') {
			const cells = [...body.matchAll(/<t[dh]\b[^>]*>([\s\S]*?)<\/t[dh]>/gi)].map((c) =>
				inlineText(c[1]).replace(/\n+/g, ' ')
			);
			// The match ends after </tr>, so the cells' own paragraphs are never visited twice.
			if (cells.some(Boolean)) blocks.push(cells.join(' | '));
			continue;
		}
		const text = inlineText(body);
		if (!text) continue;
		if (tag[0] === 'h') blocks.push('#'.repeat(Number(tag[1])) + ' ' + text);
		else if (tag === 'li') blocks.push('- ' + text);
		else blocks.push(text);
	}
	return blocks.join('\n\n');
}

/**
 * Sheet rows as records that stand on their own: `Header: value | Header: value`. The first
 * non-empty row is taken as the header; without one, cells are written plainly.
 */
export function rowsToRecords(rows: unknown[][]): string {
	const clean = rows
		.map((row) => row.map((cell) => (cell === null || cell === undefined ? '' : String(cell).trim())))
		.filter((row) => row.some(Boolean));
	if (clean.length === 0) return '';
	const [header, ...body] = clean;
	const named = header.filter(Boolean).length >= Math.max(1, header.length / 2);
	const lines = (named ? body : clean).map((row) =>
		row
			.map((cell, i) => (cell && named && header[i] ? `${header[i]}: ${cell}` : cell))
			.filter(Boolean)
			.join(' | ')
	);
	return lines.filter(Boolean).join('\n');
}

async function readBuffer(file: File): Promise<ArrayBuffer> {
	return file.arrayBuffer();
}

export async function extractDocxSegments(file: File): Promise<DocumentSegment[]> {
	const mammoth = await import('mammoth');
	const { value } = await mammoth.convertToHtml({ arrayBuffer: await readBuffer(file) });
	const text = docxHtmlToText(value);
	return text ? [{ text }] : [];
}

export async function extractSheetSegments(file: File, csv = false): Promise<DocumentSegment[]> {
	const XLSX = await import('xlsx');
	const workbook = csv
		? XLSX.read(await file.text(), { type: 'string' })
		: XLSX.read(new Uint8Array(await readBuffer(file)), { type: 'array' });
	const segments: DocumentSegment[] = [];
	for (const name of workbook.SheetNames) {
		const rows = XLSX.utils.sheet_to_json<unknown[]>(workbook.Sheets[name], {
			header: 1,
			raw: false,
			blankrows: false
		});
		const text = rowsToRecords(rows);
		if (text) segments.push({ label: csv ? file.name : name, text, records: true });
	}
	return segments;
}

export function segmentsText(segments: DocumentSegment[]): string {
	// Page markers let the model cite pages, and the viewer open a file sent whole at one.
	return segments
		.map((s) =>
			s.page
				? `[Page ${s.page}]\n${s.text}`
				: s.label && segments.length > 1
					? `## ${s.label}\n${s.text}`
					: s.text
		)
		.join('\n\n');
}
