import { expect, it } from 'vitest';
import { checkPdfPageCount, MAX_PDF_PAGES, PdfTooLongError } from './pdf-page-limits';

it('accepts a PDF at the page limit', () => {
	expect(() => checkPdfPageCount(MAX_PDF_PAGES)).not.toThrow();
});

it('refuses a longer PDF with a message the user can act on', () => {
	expect(() => checkPdfPageCount(1001)).toThrow(PdfTooLongError);
	expect(() => checkPdfPageCount(4200)).toThrow(/4,200 pages; Delta reads up to 1,000\. Split it/);
});
