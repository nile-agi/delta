import { describe, expect, it } from 'vitest';
import { convertPDFToImage, extractPDFPages } from './pdf-processing';

// A two-page PDF built by hand, so the real PDF.js parses it in the browser.
function tinyPdf(pages: string[]): File {
	const objects: string[] = [];
	const kids = pages.map((_, i) => `${4 + i * 2} 0 R`).join(' ');
	objects.push('<< /Type /Catalog /Pages 2 0 R >>');
	objects.push(`<< /Type /Pages /Kids [${kids}] /Count ${pages.length} >>`);
	objects.push('<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>');
	pages.forEach((text, i) => {
		const stream = `BT /F1 18 Tf 50 700 Td (${text}) Tj ET`;
		objects.push(
			`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents ${5 + i * 2} 0 R >>`
		);
		objects.push(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
	});
	let pdf = '%PDF-1.4\n';
	const offsets: number[] = [];
	objects.forEach((body, i) => {
		offsets.push(pdf.length);
		pdf += `${i + 1} 0 obj\n${body}\nendobj\n`;
	});
	const xref = pdf.length;
	pdf += `xref\n0 ${objects.length + 1}\n0000000000 65535 f \n`;
	for (const offset of offsets) pdf += `${String(offset).padStart(10, '0')} 00000 n \n`;
	pdf += `trailer\n<< /Size ${objects.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
	return new File([pdf], 'tiny.pdf', { type: 'application/pdf' });
}

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
});
