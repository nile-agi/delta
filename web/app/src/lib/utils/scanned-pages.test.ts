import { beforeEach, expect, it, vi } from 'vitest';
const m = vi.hoisted(() => ({
	status: vi.fn(),
	download: vi.fn(),
	readPage: vi.fn(),
	pageImage: vi.fn(),
	destroy: vi.fn()
}));
vi.mock('$lib/services/ocr', () => ({
	ocrService: { status: m.status, download: m.download, cancel: vi.fn(), readPage: m.readPage }
}));
vi.mock('./pdf-processing', () => ({
	openPDF: async () => ({ loadingTask: { destroy: m.destroy } }),
	pageImage: m.pageImage
}));
vi.mock('svelte-sonner', () => ({
	toast: {
		loading: vi.fn(() => 't1'),
		success: vi.fn(),
		warning: vi.fn(),
		error: vi.fn(),
		info: vi.fn(),
		dismiss: vi.fn()
	}
}));
import { toast } from 'svelte-sonner';
import { formatSize, mergeOcrPages, readScannedPages, scannedPages } from './scanned-pages';

const status = (installed: boolean, extra = {}) => ({
	model: { id: 'glm-ocr', display_name: 'GLM-OCR', size_bytes: 1434837056 },
	installed,
	downloading: false,
	progress: 0,
	current_bytes: 0,
	total_bytes: 1434837056,
	...extra
});
const pages = [
	'A full page of real report text about boreholes.',
	'',
	'12',
	'Another page with plenty of words.'
];

beforeEach(() => {
	vi.clearAllMocks();
	m.pageImage.mockImplementation(async (_pdf, n: number) => `data:image/png;base64,page${n}`);
	m.readPage.mockImplementation(
		async (image: string) => `Scanned text read from ${image.slice(-5)}`
	);
});

it('finds pages with almost no text, a lone page number included', () => {
	expect(scannedPages(pages)).toEqual([2, 3]);
	expect(scannedPages([])).toEqual([]);
});

it('fills scanned pages in place and keeps real text', () => {
	expect(mergeOcrPages(['real text', '', '7'], { 2: 'read', 3: '' })).toEqual([
		'real text',
		'read',
		'7'
	]);
	expect(formatSize(1434837056)).toBe('1.3 GB');
	expect(formatSize(300 * 1024 ** 2)).toBe('300 MB');
});

it('leaves a PDF with text on every page alone', async () => {
	const result = await readScannedPages('report.pdf', 'pdf', [pages[0], pages[3]]);
	expect(result).toEqual({ pages: [pages[0], pages[3]], explained: false });
	expect(m.status).not.toHaveBeenCalled();
});

it('reads only the scanned pages when the reader is downloaded', async () => {
	m.status.mockResolvedValue(status(true));
	const result = await readScannedPages('report.pdf', 'pdf', pages);
	expect(m.readPage).toHaveBeenCalledTimes(2);
	expect(result.pages).toEqual([
		pages[0],
		'Scanned text read from page2',
		'Scanned text read from page3',
		pages[3]
	]);
	expect(result.explained).toBe(false);
	expect(toast.dismiss).toHaveBeenCalledWith('t1');
	expect(m.destroy).toHaveBeenCalled();
});

it('offers the reader, with its size, when it is not downloaded', async () => {
	m.status.mockResolvedValue(status(false));
	const result = await readScannedPages('scan.pdf', 'pdf', pages);
	expect(result).toEqual({ pages, explained: true });
	expect(m.readPage).not.toHaveBeenCalled();
	const [message, options] = vi.mocked(toast.warning).mock.calls[0] as [
		string,
		Record<string, any>
	];
	expect(message).toBe(`"scan.pdf" has 2 scanned pages Delta can't read yet.`);
	expect(options.description).toContain('1.3 GB');
	expect(options.action.label).toBe('Download');
});

it('keeps the pages it read when the reader fails part way', async () => {
	m.status.mockResolvedValue(status(true));
	m.readPage
		.mockResolvedValueOnce('First scan')
		.mockRejectedValueOnce(new Error('The text reader did not answer.'));
	const result = await readScannedPages('scan.pdf', 'pdf', pages);
	expect(result.pages).toEqual([pages[0], 'First scan', '12', pages[3]]);
	expect(result.explained).toBe(true);
	expect(toast.error).toHaveBeenCalledWith(
		expect.stringContaining('could not be read'),
		expect.anything()
	);
});

it('sends the file as it is when the engine has no reader', async () => {
	m.status.mockRejectedValue(new Error('404'));
	expect(await readScannedPages('scan.pdf', 'pdf', pages)).toEqual({ pages, explained: false });
});
