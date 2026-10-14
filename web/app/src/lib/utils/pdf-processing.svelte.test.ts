import { describe, expect, it } from 'vitest';
import { convertPDFToImage, extractPDFPages, openPDF, pageImage } from './pdf-processing';
import { tinyPdf, tinyPdfBase64 } from '../../tests/tiny-pdf';

describe('PDF.js in the browser', () => {
	it('extracts each page of a real PDF in order', async () => {
		const pages = await extractPDFPages(tinyPdf(['Pump warranty ends 2027', 'Second page text']));
		expect(pages).toEqual(['Pump warranty ends 2027', 'Second page text']);
	});

	it('renders pages to images', async () => {
		const images = await convertPDFToImage(tinyPdf(['Hello']), 0.5);
		expect(images).toHaveLength(1);
		expect(images[0]).toMatch(/^data:image\/png;base64,/);
	});

	it('renders a page for the text reader at a fixed long side', async () => {
		const pdf = await openPDF(tinyPdfBase64(['Scanned']));
		const img = new Image();
		img.src = await pageImage(pdf, 1, 600);
		await img.decode();
		expect(Math.max(img.naturalWidth, img.naturalHeight)).toBe(600);
	});
});
