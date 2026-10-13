import { describe, expect, it } from 'vitest';
import {
	bestPageFor,
	citationLabel,
	findAttachedDocument,
	highlightRanges,
	matchPassage,
	resolveCitation,
	sectionIndexFor,
	sectionsFromContent,
	splitCitations,
	words
} from './citations';

describe('splitCitations', () => {
	it('finds page, page-range and section citations', () => {
		const parts = splitCitations(
			'Every 400 hours [service.pdf, p.17]. Seals [manual.pdf, p.41-42] and [manual.pdf, pp. 3–4]; see [notes.md, section 4].'
		);
		const cites = parts.filter((p) => typeof p !== 'string');
		expect(cites).toEqual([
			{
				name: 'service.pdf',
				pageStart: 17,
				pageEnd: 17,
				sectionStart: undefined,
				sectionEnd: undefined
			},
			{
				name: 'manual.pdf',
				pageStart: 41,
				pageEnd: 42,
				sectionStart: undefined,
				sectionEnd: undefined
			},
			{
				name: 'manual.pdf',
				pageStart: 3,
				pageEnd: 4,
				sectionStart: undefined,
				sectionEnd: undefined
			},
			{ name: 'notes.md', pageStart: undefined, pageEnd: undefined, sectionStart: 4, sectionEnd: 4 }
		]);
		expect(parts[0]).toBe('Every 400 hours ');
		expect(parts.join('')).toContain('. Seals ');
	});

	it('leaves links, plain brackets and text alone', () => {
		expect(splitCitations('A [link](http://x) and [just text] here')).toEqual([
			'A [link](http://x) and [just text] here'
		]);
	});
});

describe('citationLabel', () => {
	it('names pages or sections', () => {
		expect(citationLabel({ name: 'a', pageStart: 41, pageEnd: 42 })).toBe('p.41–42');
		expect(citationLabel({ name: 'a', pageStart: 7, pageEnd: 7 })).toBe('p.7');
		expect(citationLabel({ name: 'a', sectionStart: 4 })).toBe('section 4');
	});
});

const source = (over: Partial<AgentSource> = {}): AgentSource => ({
	document: 'manual.pdf',
	document_id: 'doc-1',
	source: 'manual.pdf, p.41-42',
	page_start: 41,
	page_end: 42,
	text: 'The hydraulic pump warranty expires after 36 months.',
	...over
});

describe('resolveCitation', () => {
	it('fills in the document id and passage from the matching source', () => {
		const t = resolveCitation({ name: 'Manual.pdf', pageStart: 42, pageEnd: 42 }, [
			source({ page_start: 3, page_end: 3, text: 'other' }),
			source()
		]);
		expect(t.docId).toBe('doc-1');
		expect(t.passage).toContain('36 months');
	});

	it('keeps the id but drops a passage from another page', () => {
		const t = resolveCitation({ name: 'manual.pdf', pageStart: 9, pageEnd: 9 }, [source()]);
		expect(t.docId).toBe('doc-1');
		expect(t.passage).toBeUndefined();
	});
});

describe('findAttachedDocument', () => {
	const messages = [
		{
			role: 'user',
			extra: [
				{
					type: 'documentFile',
					name: 'manual.pdf',
					mimeType: 'application/pdf',
					docId: 'doc-1',
					base64Data: 'QUJD',
					pages: 60,
					chars: 1
				},
				{ type: 'textFile', name: 'notes.txt', content: 'hello' }
			]
		}
	] as unknown as DatabaseMessage[];

	it('finds a file by id or by name', () => {
		expect(findAttachedDocument(messages, { name: 'x', docId: 'doc-1' })?.base64Data).toBe('QUJD');
		expect(findAttachedDocument(messages, { name: 'NOTES.txt' })?.content).toBe('hello');
	});

	it('falls back to the engine for a kept file not in this chat', () => {
		expect(findAttachedDocument(messages, { name: 'kept.docx', docId: 'doc-9' })).toEqual({
			name: 'kept.docx',
			docId: 'doc-9'
		});
		expect(findAttachedDocument(messages, { name: 'missing.pdf' })).toBeNull();
	});
});

describe('sections', () => {
	const content = '[Page 1]\nIntro text.\n\n[Page 2]\nThe filter is replaced every 400 hours.';

	it('splits text sent with a message back into pages', () => {
		const sections = sectionsFromContent(content);
		expect(sections.map((s) => s.pageStart)).toEqual([1, 2]);
		expect(sections[1].text).toContain('400 hours');
		expect(sectionsFromContent('no markers')).toEqual([{ section: 1, text: 'no markers' }]);
	});

	it('finds the section for a page, a section number or the passage', () => {
		const sections = sectionsFromContent(content);
		expect(sectionIndexFor(sections, { name: 'a', pageStart: 2 })).toBe(1);
		expect(sectionIndexFor(sections, { name: 'a', sectionStart: 1 })).toBe(0);
		expect(sectionIndexFor(sections, { name: 'a', passage: 'filter is replaced every 400' })).toBe(
			1
		);
		expect(sectionIndexFor(sections, { name: 'a', pageStart: 9 })).toBe(-1);
	});

	it('prefers the section holding the passage when a source spans pages', () => {
		const sections = sectionsFromContent(content);
		expect(
			sectionIndexFor(sections, {
				name: 'a',
				pageStart: 1,
				pageEnd: 2,
				passage: 'filter is replaced every 400'
			})
		).toBe(1);
		expect(sectionIndexFor(sections, { name: 'a', pageStart: 1, pageEnd: 2 })).toBe(0);
	});

	it('picks the page that holds most of the passage', () => {
		expect(
			bestPageFor(
				['intro text here', 'the filter is replaced every 400 hours', 'other'],
				'filter is replaced every 400 hours'
			)
		).toBe(1);
		expect(bestPageFor(['a', 'b'], 'nothing matches here at all')).toBe(0);
	});
});

describe('passage matching', () => {
	it('marks the words of the passage despite line breaks and hyphens', () => {
		const page = words(
			'Intro. The compressor filter must be re-\nplaced every 400 operating hours. Other text.'
		).map((w) => w.word);
		const marked = matchPassage(
			page,
			'The compressor filter must be re- placed every 400 operating hours.'
		);
		const hit = page.filter((_, i) => marked[i]);
		expect(hit).toContain('compressor');
		expect(hit).toContain('400');
		expect(hit).not.toContain('intro');
		expect(hit).not.toContain('other');
	});

	it('gives merged character ranges to highlight', () => {
		const text = 'Intro. The pump warranty expires after 36 months. Other text.';
		const ranges = highlightRanges(text, 'pump warranty expires after 36 months');
		expect(ranges).toHaveLength(1);
		expect(text.slice(...ranges[0])).toBe('pump warranty expires after 36 months');
		expect(highlightRanges(text, 'nothing here matches at all')).toEqual([]);
	});
});
