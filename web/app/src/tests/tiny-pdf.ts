/** A small PDF built by hand, one line of text per page, so tests can run the real PDF.js on it. */
export function tinyPdfText(pages: string[]): string {
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
	return pdf;
}

export function tinyPdf(pages: string[]): File {
	return new File([tinyPdfText(pages)], 'tiny.pdf', { type: 'application/pdf' });
}

export function tinyPdfBase64(pages: string[]): string {
	return btoa(tinyPdfText(pages));
}
