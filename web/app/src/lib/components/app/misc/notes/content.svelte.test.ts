import { expect, it } from 'vitest';
import { prepareContent, serializeContent } from './content';

it('escapes plain text instead of interpreting it as markup', () => {
	expect(prepareContent('hello < 3 & goodbye\nnext')).toBe(
		'<p>hello &lt; 3 &amp; goodbye</p><p>next</p>'
	);
});
it('converts legacy checked items and highlights', () => {
	const html = prepareContent(
		'<ul class="checklist"><li><input type="checkbox" checked><span>Done</span></li></ul><span class="highlighted-text">Important</span>'
	);
	expect(html).toContain('data-checked="true"');
	expect(html).toContain('<mark>Important</mark>');
});
it('round trips unsupported legacy content rather than dropping it', () => {
	const original = '<p>Before</p><iframe src="https://example.com">Example</iframe><p>After</p>';
	expect(serializeContent(prepareContent(original))).toBe(original);
});

it('preserves legacy inline markup after a leading text node', () => {
	expect(prepareContent('Hello <b>world</b>')).toBe('Hello <b>world</b>');
});
