// Past this a PDF would hold the app for hours (each scanned page takes the reader seconds).
export const MAX_PDF_PAGES = 1_000;

/** A PDF too long to attach; its message is shown to the user as it is. */
export class PdfTooLongError extends Error {
	constructor(pages: number) {
		super(
			`This PDF has ${pages.toLocaleString()} pages; Delta reads up to ${MAX_PDF_PAGES.toLocaleString()}. Split it into smaller files.`
		);
		this.name = 'PdfTooLongError';
	}
}

/** Throws before any page is read when a PDF is over the page limit. */
export function checkPdfPageCount(pages: number): void {
	if (pages > MAX_PDF_PAGES) throw new PdfTooLongError(pages);
}
