import { toast } from 'svelte-sonner';
import { ocrService, type OcrStatus } from '$lib/services/ocr';
import { openPDF, pageImage } from './pdf-processing';

// A page with fewer letters and digits than this has no real text layer (a scan, maybe a page number).
const MIN_PAGE_CHARS = 20;
// Twice PDF size (about 144 DPI): sharp enough for small print, small enough to send quickly.
const OCR_SCALE = 2;
const POLL_MS = 1000;
const DOWNLOAD_TOAST = 'ocr-download';

/** The pages (1-based) that have almost no extracted text, so are likely scans. */
export function scannedPages(pages: string[]): number[] {
	const out: number[] = [];
	pages.forEach((text, i) => {
		if ((text.match(/[\p{L}\p{N}]/gu)?.length ?? 0) < MIN_PAGE_CHARS) out.push(i + 1);
	});
	return out;
}

/** Pages with the read text in place of what extraction found; page numbers stay the same. */
export function mergeOcrPages(pages: string[], read: Record<number, string>): string[] {
	return pages.map((text, i) => {
		const ocr = read[i + 1]?.trim();
		return ocr && ocr.length > text.trim().length ? ocr : text;
	});
}

export function formatSize(bytes: number): string {
	const gb = bytes / 1024 ** 3;
	return gb >= 1 ? `${gb.toFixed(1)} GB` : `${Math.round(bytes / 1024 ** 2)} MB`;
}

export interface ScannedPagesResult {
	pages: string[];
	/** The user was told why the scanned pages are missing (offered the reader, or it failed). */
	explained: boolean;
}

/**
 * Reads a PDF's scanned pages with the text reader when it is downloaded; otherwise offers to
 * download it. Pages that cannot be read keep what extraction found.
 */
export async function readScannedPages(
	name: string,
	base64Data: string,
	pages: string[]
): Promise<ScannedPagesResult> {
	const scanned = scannedPages(pages);
	if (scanned.length === 0) return { pages, explained: false };
	let status: OcrStatus;
	try {
		status = await ocrService.status();
	} catch {
		return { pages, explained: false }; // an engine without the reader
	}
	if (!status.installed) {
		offerTextReader(name, scanned.length, status);
		return { pages, explained: true };
	}

	const controller = new AbortController();
	const id = toast.loading(progressText(0, scanned.length), {
		description: name,
		action: { label: 'Stop', onClick: () => controller.abort() }
	});
	const read: Record<number, string> = {};
	try {
		const pdf = await openPDF(base64Data);
		try {
			for (const [i, n] of scanned.entries()) {
				if (controller.signal.aborted) break;
				toast.loading(progressText(i, scanned.length), { id, description: name });
				read[n] = await ocrService.readPage(await pageImage(pdf, n, OCR_SCALE), controller.signal);
			}
		} finally {
			void pdf.loadingTask.destroy();
		}
	} catch (error) {
		if (!controller.signal.aborted) {
			console.error('Could not read scanned pages:', error);
			toast.error(`Some scanned pages of "${name}" could not be read.`, {
				id,
				description: error instanceof Error ? error.message : undefined
			});
			return { pages: mergeOcrPages(pages, read), explained: true };
		}
	}
	const done = Object.keys(read).length;
	if (controller.signal.aborted)
		toast.info(`Stopped after ${done} of ${scanned.length} scanned pages.`, {
			id,
			description: name
		});
	else toast.dismiss(id);
	return { pages: mergeOcrPages(pages, read), explained: controller.signal.aborted };
}

function progressText(done: number, total: number): string {
	return total === 1 ? 'Reading a scanned page…' : `Reading scanned pages ${done + 1} of ${total}…`;
}

/** Tells the user why scanned pages were skipped, with a one-click download of the reader. */
export function offerTextReader(name: string, count: number, status: OcrStatus) {
	if (status.downloading) {
		void followDownload();
		return;
	}
	const pages = count === 1 ? 'a scanned page' : `${count} scanned pages`;
	toast.warning(`"${name}" has ${pages} Delta can't read yet.`, {
		description: `Download the text reader once (${formatSize(status.model.size_bytes)}) and Delta reads scanned pages on this computer.`,
		duration: 15000,
		action: { label: 'Download', onClick: () => void downloadTextReader() }
	});
}

export async function downloadTextReader() {
	try {
		await ocrService.download();
		await followDownload();
	} catch (error) {
		toast.error('The text reader could not be downloaded.', {
			id: DOWNLOAD_TOAST,
			description: error instanceof Error ? error.message : undefined
		});
	}
}

let following = false;

async function followDownload() {
	if (following) return;
	following = true;
	try {
		for (;;) {
			const s = await ocrService.status();
			if (s.installed) {
				toast.success('Text reader ready', {
					id: DOWNLOAD_TOAST,
					description: 'Attach the file again to read its scanned pages.'
				});
				return;
			}
			if (!s.downloading) {
				toast.error(s.error || 'The text reader download stopped.', {
					id: DOWNLOAD_TOAST,
					action: { label: 'Retry', onClick: () => void downloadTextReader() }
				});
				return;
			}
			toast.loading(`Downloading the text reader… ${Math.floor(s.progress)}%`, {
				id: DOWNLOAD_TOAST,
				description: `${formatSize(s.current_bytes)} of ${formatSize(s.total_bytes)}`,
				action: { label: 'Cancel', onClick: () => void ocrService.cancel() }
			});
			await new Promise((r) => setTimeout(r, POLL_MS));
		}
	} catch (error) {
		console.error('Lost track of the text reader download:', error);
	} finally {
		following = false;
	}
}
