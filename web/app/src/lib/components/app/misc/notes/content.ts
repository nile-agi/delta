import { Node, mergeAttributes } from '@tiptap/core';

/** Preserve unsupported legacy blocks without silently discarding them on the next edit. */
export const LegacyBlock = Node.create({
	name: 'legacyBlock',
	group: 'block',
	atom: true,
	addAttributes() {
		return { html: { default: '', parseHTML: (el) => el.getAttribute('data-legacy-html') } };
	},
	parseHTML() {
		return [{ tag: 'div[data-legacy-html]' }];
	},
	renderText({ node }) {
		return new DOMParser().parseFromString(node.attrs.html, 'text/html').body.textContent || '';
	},
	renderHTML({ node }) {
		const doc = new DOMParser().parseFromString(node.attrs.html, 'text/html');
		return [
			'div',
			{ 'data-legacy-html': node.attrs.html, class: 'legacy-block', contenteditable: 'false' },
			doc.body.textContent || 'Preserved embedded content'
		];
	}
});

export const Attachment = Node.create({
	name: 'attachment',
	group: 'block',
	atom: true,
	addAttributes() {
		return {
			name: {
				default: 'Attachment',
				parseHTML: (el) =>
					el.querySelector('.file-header')?.textContent || el.getAttribute('data-name')
			},
			text: { default: '', parseHTML: (el) => el.querySelector('pre')?.textContent || '' }
		};
	},
	parseHTML() {
		return [{ tag: 'div.file-attachment' }];
	},
	renderText({ node }) {
		return `${node.attrs.name}\n${node.attrs.text}`;
	},
	renderHTML({ node, HTMLAttributes }) {
		return [
			'div',
			mergeAttributes(HTMLAttributes, { class: 'file-attachment' }),
			['div', { class: 'file-header' }, node.attrs.name],
			['pre', {}, node.attrs.text]
		];
	}
});

export function prepareContent(content: string): string {
	const doc = document.implementation.createHTMLDocument();
	if (!/<\/?[a-z][^>]*>/i.test(content)) {
		for (const line of content.split('\n')) {
			const p = doc.createElement('p');
			p.textContent = line;
			doc.body.append(p);
		}
		return doc.body.innerHTML;
	}
	doc.body.innerHTML = content;
	for (const ul of doc.querySelectorAll('ul.checklist')) {
		ul.setAttribute('data-type', 'taskList');
		for (const li of ul.querySelectorAll(':scope > li')) {
			li.setAttribute('data-type', 'taskItem');
			li.setAttribute('data-checked', String(!!li.querySelector('input[checked]')));
			li.querySelector('input')?.remove();
			const p = doc.createElement('p');
			p.innerHTML = li.innerHTML;
			li.replaceChildren(p);
		}
	}
	for (const span of doc.querySelectorAll(
		'span.highlighted-text, span[style*="background-color"]'
	)) {
		const mark = doc.createElement('mark');
		mark.innerHTML = span.innerHTML;
		span.replaceWith(mark);
	}
	const supported = new Set([
		'P',
		'DIV',
		'BR',
		'STRONG',
		'B',
		'EM',
		'I',
		'U',
		'S',
		'STRIKE',
		'MARK',
		'H1',
		'H2',
		'H3',
		'UL',
		'OL',
		'LI',
		'BLOCKQUOTE',
		'TABLE',
		'THEAD',
		'TBODY',
		'TR',
		'TD',
		'TH',
		'IMG',
		'PRE',
		'CODE',
		'SPAN',
		'LABEL',
		'INPUT'
	]);
	for (const element of Array.from(doc.body.querySelectorAll('*'))) {
		if (!doc.body.contains(element) || element.closest('.file-attachment, [data-legacy-html]'))
			continue;
		if (!supported.has(element.tagName)) {
			const block = doc.createElement('div');
			block.setAttribute('data-legacy-html', element.outerHTML);
			block.textContent = element.textContent;
			element.replaceWith(block);
		}
	}
	return doc.body.innerHTML;
}

export function serializeContent(html: string): string {
	const doc = new DOMParser().parseFromString(html, 'text/html');
	for (const block of doc.querySelectorAll('[data-legacy-html]')) {
		block.outerHTML = block.getAttribute('data-legacy-html') || '';
	}
	return doc.body.innerHTML;
}
