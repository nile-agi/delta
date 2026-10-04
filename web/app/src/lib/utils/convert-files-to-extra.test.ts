import { beforeEach, afterEach, expect, it, vi } from 'vitest';
const pdf = vi.hoisted(() => ({ text: vi.fn(), images: vi.fn(), vision: false }));
vi.mock('./pdf-processing', () => ({ convertPDFToText: pdf.text, convertPDFToImage: pdf.images }));
vi.mock('$lib/stores/server.svelte', () => ({ supportsVision: () => pdf.vision }));
vi.mock('$lib/stores/settings.svelte', () => ({
	config: () => ({ pdfAsImage: false }),
	settingsStore: { updateConfig: vi.fn() }
}));
vi.mock('svelte-sonner', () => ({ toast: { success: vi.fn(), warning: vi.fn(), error: vi.fn() } }));
import { parseFilesToMessageExtras } from './convert-files-to-extra';
beforeEach(() => {
	pdf.vision = false;
	pdf.text.mockReset().mockResolvedValue('Document text');
	pdf.images.mockReset().mockResolvedValue(['data:image/png;base64,page']);
	vi.stubGlobal(
		'FileReader',
		class {
			result = 'data:application/pdf;base64,pdf';
			onload?: () => void;
			readAsDataURL() {
				this.onload?.();
			}
		}
	);
});
afterEach(() => vi.unstubAllGlobals());
const file = {
	name: 'document.pdf',
	type: 'application/pdf',
	file: new File(['pdf'], 'document.pdf', { type: 'application/pdf' })
} as ChatUploadedFile;
it('uses extracted text even when the model can read images', async () => {
	pdf.vision = true;
	const result = await parseFilesToMessageExtras([file]);
	expect(result.extras[0]).toMatchObject({ content: 'Document text', processedAsImages: false });
	expect(pdf.images).not.toHaveBeenCalled();
});
it('automatically reads a scanned PDF as images with a capable model', async () => {
	pdf.vision = true;
	pdf.text.mockResolvedValue('');
	const result = await parseFilesToMessageExtras([file]);
	expect(result.extras[0]).toMatchObject({
		processedAsImages: true,
		images: ['data:image/png;base64,page']
	});
});
it('explains why a scanned PDF cannot be read by a text-only model', async () => {
	pdf.text.mockResolvedValue('');
	const result = await parseFilesToMessageExtras([file]);
	expect(result.extras).toEqual([]);
	expect(pdf.images).not.toHaveBeenCalled();
	const { toast } = await import('svelte-sonner');
	expect(toast.warning).toHaveBeenCalledWith(expect.stringContaining('model that can read images'));
});

it('uses page images if text extraction fails on a capable model', async () => {
	pdf.vision = true;
	pdf.text.mockRejectedValue(new Error('No text layer'));
	const result = await parseFilesToMessageExtras([file]);
	expect(result.extras[0]).toMatchObject({ processedAsImages: true });
});
it('reports a processing failure instead of sending an empty attachment', async () => {
	pdf.vision = true;
	pdf.text.mockResolvedValue('');
	pdf.images.mockRejectedValue(new Error('Invalid PDF'));
	const result = await parseFilesToMessageExtras([file]);
	expect(result.extras).toEqual([]);
	const { toast } = await import('svelte-sonner');
	expect(toast.error).toHaveBeenCalledWith(expect.stringContaining('Could not read'));
});
