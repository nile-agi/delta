import { describe, expect, it } from 'vitest';
import { docxHtmlToText, documentKind, rowsToRecords, segmentsText } from './document-extraction';

describe('documentKind', () => {
	it('recognises Word and spreadsheet files by type or, when the type is blank, by name', () => {
		expect(
			documentKind(
				'a.docx',
				'application/vnd.openxmlformats-officedocument.wordprocessingml.document'
			)
		).toBe('docx');
		expect(documentKind('Report.DOCX', '')).toBe('docx');
		expect(documentKind('budget.xlsx', '')).toBe('sheet');
		expect(documentKind('old.xls', 'application/vnd.ms-excel')).toBe('sheet');
		expect(documentKind('data.csv', 'text/csv')).toBe('csv');
		expect(documentKind('notes.md', 'text/markdown')).toBeNull();
		expect(documentKind('paper.pdf', 'application/pdf')).toBeNull();
	});
});

describe('docxHtmlToText', () => {
	it('keeps headings, lists and table rows as structure retrieval can use', () => {
		const html =
			'<h1>Safety &amp; Rules</h1><p>Wear a <strong>helmet</strong>.</p>' +
			'<ul><li>Gloves</li><li>Boots</li></ul>' +
			'<table><tr><td><p>Item</p></td><td><p>Cost</p></td></tr><tr><td><p>Pump</p></td><td><p>120</p></td></tr></table>' +
			'<h2>Contacts</h2><p>Call&nbsp;ext. 42</p>';
		expect(docxHtmlToText(html)).toBe(
			[
				'# Safety & Rules',
				'Wear a helmet.',
				'- Gloves',
				'- Boots',
				'Item | Cost',
				'Pump | 120',
				'## Contacts',
				'Call ext. 42'
			].join('\n\n')
		);
	});

	it('drops empty paragraphs', () => {
		expect(docxHtmlToText('<p></p><p> </p><p>Text</p>')).toBe('Text');
	});
});

describe('rowsToRecords', () => {
	it('writes each row as header: value pairs that stand on their own', () => {
		const rows = [
			['Name', 'Price', 'Stock'],
			['Pump', 120, 4],
			['Seal', null, 30],
			[null, null, null]
		];
		expect(rowsToRecords(rows)).toBe('Name: Pump | Price: 120 | Stock: 4\nName: Seal | Stock: 30');
	});

	it('writes cells plainly when the first row is not a header', () => {
		expect(rowsToRecords([['', '', 'x'], ['a', 'b', 'c']])).toBe('x\na | b | c');
	});

	it('returns nothing for an empty sheet', () => {
		expect(rowsToRecords([[], [null]])).toBe('');
	});
});

describe('segmentsText', () => {
	it('labels sheets only when there is more than one', () => {
		expect(segmentsText([{ label: 'Only', text: 'a' }])).toBe('a');
		expect(
			segmentsText([
				{ label: 'One', text: 'a' },
				{ label: 'Two', text: 'b' }
			])
		).toBe('## One\na\n\n## Two\nb');
	});
});
