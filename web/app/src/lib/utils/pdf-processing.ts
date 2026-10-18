/**
 * PDF processing utilities using PDF.js
 * Handles PDF text extraction and image conversion in the browser
 */

import { browser } from '$app/environment';
import { MimeTypeApplication, MimeTypeImage } from '$lib/enums/files';
// Desktop WebKit versions without Iterator must also be able to import the app.
// PDF.js ships its supported compatibility build and matching worker for these runtimes.
import * as pdfjs from 'pdfjs-dist/legacy/build/pdf.mjs';

type TextItem = {
	str?: string;
	hasEOL?: boolean;
	height?: number;
	transform?: number[];
};

// The worker is bundled as text and loaded once; every PDF call waits for it, since a file
// opened before it was ready used to fail with "No GlobalWorkerOptions.workerSrc specified".
const workerReady: Promise<void> = browser
	? import('pdfjs-dist/legacy/build/pdf.worker.min.mjs?raw')
			.then((workerModule) => {
				const workerBlob = new Blob([workerModule.default], { type: 'application/javascript' });
				pdfjs.GlobalWorkerOptions.workerSrc = URL.createObjectURL(workerBlob);
			})
			.catch(() => {
				console.warn('Failed to load PDF.js worker, PDF processing may not work');
			})
	: Promise.resolve();

/**
 * Convert a File object to ArrayBuffer for PDF.js processing
 * @param file - The PDF file to convert
 * @returns Promise resolving to the file's ArrayBuffer
 */
async function getFileAsBuffer(file: File): Promise<ArrayBuffer> {
	return new Promise((resolve, reject) => {
		const reader = new FileReader();
		reader.onload = (event) => {
			if (event.target?.result) {
				resolve(event.target.result as ArrayBuffer);
			} else {
				reject(new Error('Failed to read file.'));
			}
		};
		reader.onerror = () => {
			reject(new Error('Failed to read file.'));
		};
		reader.readAsArrayBuffer(file);
	});
}

/**
 * Rebuild a page's text from PDF.js items. Items are runs of glyphs, not lines, so joining them
 * with newlines (as this used to) split sentences mid-way. A new line starts where the baseline
 * moves; a gap of more than about one and a half lines is a paragraph break.
 */
export function pageItemsToText(items: TextItem[]): string {
	let text = '';
	let lastY: number | null = null;
	let lastHeight = 0;
	for (const item of items) {
		if (typeof item.str !== 'string') continue;
		const y = item.transform?.[5];
		if (lastY !== null && y !== undefined && Math.abs(y - lastY) > 1 && !text.endsWith('\n')) {
			const gap = Math.abs(y - lastY);
			text += gap > Math.max(lastHeight, item.height ?? 0) * 1.6 ? '\n\n' : '\n';
		}
		text += item.str;
		if (item.hasEOL) text += '\n';
		if (y !== undefined) lastY = y;
		if (item.height) lastHeight = item.height;
	}
	return text
		.replace(/[ \t]+\n/g, '\n')
		.replace(/\n{3,}/g, '\n\n')
		.trim();
}

/**
 * Extract the text of every page of a PDF, in order
 * @param file - The PDF file to process
 * @returns Promise resolving to one string per page
 */
export async function extractPDFPages(file: File): Promise<string[]> {
	if (!browser) {
		throw new Error('PDF processing is only available in the browser');
	}

	try {
		const buffer = await getFileAsBuffer(file);
		await workerReady;
		const pdf = await pdfjs.getDocument({ data: new Uint8Array(buffer) }).promise;
		const pages: Promise<string>[] = [];
		for (let i = 1; i <= pdf.numPages; i++) {
			pages.push(
				pdf
					.getPage(i)
					// eslint-disable-next-line @typescript-eslint/no-explicit-any
					.then((page: any) => page.getTextContent())
					.then((content: { items: TextItem[] }) => pageItemsToText(content.items))
			);
		}
		return await Promise.all(pages);
	} catch (error) {
		console.error('Error converting PDF to text:', error);
		throw new Error(
			`Failed to convert PDF to text: ${error instanceof Error ? error.message : 'Unknown error'}`
		);
	}
}

/**
 * Extract text content from a PDF file
 * @param file - The PDF file to process
 * @returns Promise resolving to the extracted text content
 */
export async function convertPDFToText(file: File): Promise<string> {
	return (await extractPDFPages(file)).join('\n\n');
}

/**
 * Convert PDF pages to PNG images as data URLs
 * @param file - The PDF file to convert
 * @param scale - Rendering scale factor (default: 1.5)
 * @returns Promise resolving to array of PNG data URLs
 */
export async function convertPDFToImage(file: File, scale: number = 1.5): Promise<string[]> {
	if (!browser) {
		throw new Error('PDF processing is only available in the browser');
	}

	try {
		const buffer = await getFileAsBuffer(file);
		await workerReady;
		const doc = await pdfjs.getDocument({ data: new Uint8Array(buffer) }).promise;
		const pages: Promise<string>[] = [];

		for (let i = 1; i <= doc.numPages; i++) {
			const page = await doc.getPage(i);
			const viewport = page.getViewport({ scale });
			const canvas = document.createElement('canvas');
			const ctx = canvas.getContext('2d');

			canvas.width = viewport.width;
			canvas.height = viewport.height;

			if (!ctx) {
				throw new Error('Failed to get 2D context from canvas');
			}

			const task = page.render({
				canvasContext: ctx,
				viewport: viewport,
				canvas: canvas
			});
			pages.push(
				task.promise.then(() => {
					return canvas.toDataURL(MimeTypeImage.PNG);
				})
			);
		}

		return await Promise.all(pages);
	} catch (error) {
		console.error('Error converting PDF to images:', error);
		throw new Error(
			`Failed to convert PDF to images: ${error instanceof Error ? error.message : 'Unknown error'}`
		);
	}
}

/** A piece of page text and the box it occupies on the rendered page, in CSS pixels. */
export interface PageTextBox {
	str: string;
	left: number;
	top: number;
	width: number;
	height: number;
}

export type OpenedPDF = Awaited<ReturnType<typeof pdfjs.getDocument>['promise']>;

/** Opens a PDF from its base64 bytes, for showing pages one at a time. */
export async function openPDF(base64Data: string): Promise<OpenedPDF> {
	if (!browser) throw new Error('PDF processing is only available in the browser');
	const binary = atob(base64Data);
	const bytes = new Uint8Array(binary.length);
	for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
	await workerReady;
	return pdfjs.getDocument({ data: bytes }).promise;
}

/** The size page `n` renders at, without drawing it. */
export async function pageSize(pdf: OpenedPDF, n: number, scale: number) {
	const viewport = (await pdf.getPage(n)).getViewport({ scale });
	return { width: viewport.width, height: viewport.height };
}

/** The text of page `n`, without drawing it. */
export async function pageText(pdf: OpenedPDF, n: number): Promise<string> {
	const content = await (await pdf.getPage(n)).getTextContent();
	return (content.items as TextItem[]).map((item) => item.str ?? '').join(' ');
}

/** Page `n` as a PNG data URL `longSide` pixels on its longer side, for the text reader. */
export async function pageImage(pdf: OpenedPDF, n: number, longSide: number): Promise<string> {
	const page = await pdf.getPage(n);
	const base = page.getViewport({ scale: 1 });
	const viewport = page.getViewport({ scale: longSide / Math.max(base.width, base.height) });
	const canvas = document.createElement('canvas');
	canvas.width = Math.floor(viewport.width);
	canvas.height = Math.floor(viewport.height);
	const ctx = canvas.getContext('2d');
	if (!ctx) throw new Error('Failed to get 2D context from canvas');
	await page.render({ canvasContext: ctx, canvas, viewport }).promise;
	return canvas.toDataURL(MimeTypeImage.PNG);
}

/** Draws page `n` into `canvas` and returns where each piece of its text sits, for highlighting. */
export async function renderPDFPage(
	pdf: OpenedPDF,
	n: number,
	canvas: HTMLCanvasElement,
	scale: number
): Promise<PageTextBox[]> {
	const page = await pdf.getPage(n);
	const viewport = page.getViewport({ scale });
	const ratio = window.devicePixelRatio || 1;
	canvas.width = Math.floor(viewport.width * ratio);
	canvas.height = Math.floor(viewport.height * ratio);
	canvas.style.width = `${viewport.width}px`;
	canvas.style.height = `${viewport.height}px`;
	const ctx = canvas.getContext('2d');
	if (!ctx) throw new Error('Failed to get 2D context from canvas');
	await page.render({
		canvasContext: ctx,
		canvas,
		viewport,
		transform: ratio !== 1 ? [ratio, 0, 0, ratio, 0, 0] : undefined
	}).promise;
	const content = await page.getTextContent();
	const boxes: PageTextBox[] = [];
	for (const item of content.items as Array<TextItem & { width?: number }>) {
		if (!item.str || !item.transform) continue;
		const tx = pdfjs.Util.transform(viewport.transform, item.transform);
		const height = Math.hypot(tx[2], tx[3]);
		boxes.push({
			str: item.str,
			left: tx[4],
			top: tx[5] - height,
			width: (item.width ?? 0) * scale,
			height
		});
	}
	return boxes;
}

/**
 * Check if a file is a PDF based on its MIME type
 * @param file - The file to check
 * @returns True if the file is a PDF
 */
export function isPdfFile(file: File): boolean {
	return file.type === MimeTypeApplication.PDF;
}

/**
 * Check if a MIME type represents a PDF
 * @param mimeType - The MIME type to check
 * @returns True if the MIME type is application/pdf
 */
export function isApplicationMimeType(mimeType: string): boolean {
	return mimeType === MimeTypeApplication.PDF;
}
