import { beforeEach, expect, it, vi } from 'vitest';

const service = vi.hoisted(() => ({ upload: vi.fn() }));
vi.mock('$lib/services/documents', () => ({ documentsService: service }));

import { indexDocumentExtras, inlineLimitChars } from './index-document-extras';

beforeEach(() => {
	service.upload.mockReset().mockResolvedValue({ id: 'doc-9', pages: 120, chars: 400000 });
});

function doc(chars: number, mimeType = 'application/pdf'): DatabaseMessageExtraDocumentFile {
	return {
		type: 'documentFile',
		name: 'manual.pdf',
		mimeType,
		pages: 2,
		chars,
		base64Data: 'cGRm',
		segments: [
			{ page: 1, text: 'a'.repeat(chars / 2) },
			{ page: 2, text: 'b'.repeat(chars / 2) }
		]
	};
}

it('sizes the inline limit from the context window, capped at about 6,000 tokens', () => {
	expect(inlineLimitChars(4096)).toBe(Math.round(4096 * 0.2 * 3.5));
	expect(inlineLimitChars(131072)).toBe(21000);
	expect(inlineLimitChars(undefined)).toBe(inlineLimitChars(4096));
});

it('sends a small PDF whole, as an ordinary PDF attachment', async () => {
	const [out] = await indexDocumentExtras([doc(200)], 'conv-1', 8192);
	expect(out).toMatchObject({ type: 'pdfFile', processedAsImages: false, base64Data: 'cGRm' });
	expect((out as DatabaseMessageExtraPdfFile).content).toContain('aaa');
	expect(service.upload).not.toHaveBeenCalled();
});

it('sends a small Word file whole, as text', async () => {
	const [out] = await indexDocumentExtras([doc(200, 'application/msword')], 'conv-1', 8192);
	expect(out).toMatchObject({ type: 'textFile', name: 'manual.pdf' });
});

it('indexes a large file and keeps only a reference', async () => {
	const [out] = await indexDocumentExtras([doc(40000)], 'conv-1', 8192);
	expect(service.upload).toHaveBeenCalledWith(
		expect.objectContaining({ name: 'manual.pdf', conversationId: 'conv-1' })
	);
	expect(out).toMatchObject({ type: 'documentFile', docId: 'doc-9', pages: 120 });
	expect(out).not.toHaveProperty('segments');
});

it('falls back to sending the file whole when indexing fails', async () => {
	service.upload.mockRejectedValue(new Error('offline'));
	const [out] = await indexDocumentExtras([doc(40000)], 'conv-1', 8192);
	expect(out).toMatchObject({ type: 'pdfFile' });
	expect((out as DatabaseMessageExtraPdfFile).content.length).toBeGreaterThan(39000);
});

it('leaves other attachments untouched', async () => {
	const text: DatabaseMessageExtra = { type: 'textFile', name: 'a.txt', content: 'hi' };
	expect(await indexDocumentExtras([text], 'conv-1', 8192)).toEqual([text]);
});
