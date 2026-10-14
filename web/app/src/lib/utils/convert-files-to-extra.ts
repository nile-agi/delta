import { convertPDFToImage, extractPDFPages } from './pdf-processing';
import {
	documentKind,
	extractDocxSegments,
	extractSheetSegments,
	type DocumentSegment
} from './document-extraction';
import { isSvgMimeType, svgBase64UrlToPngDataURL } from './svg-to-png';
import { isWebpMimeType, webpBase64UrlToPngDataURL } from './webp-to-png';
import { FileTypeCategory, MimeTypeApplication } from '$lib/enums/files';
import { supportsVision } from '$lib/stores/server.svelte';
import { getFileTypeCategory } from '$lib/utils/file-type';
import { readFileAsText, isLikelyTextFile } from './text-files';
import { toast } from 'svelte-sonner';
import { readScannedPages } from './scanned-pages';

function readFileAsBase64(file: File): Promise<string> {
	return new Promise((resolve, reject) => {
		const reader = new FileReader();

		reader.onload = () => {
			// Extract base64 data without the data URL prefix
			const dataUrl = reader.result as string;
			const base64 = dataUrl.split(',')[1];
			resolve(base64);
		};

		reader.onerror = () => reject(reader.error);

		reader.readAsDataURL(file);
	});
}

// Text files shorter than this are always sent whole; longer ones may be indexed when sent.
const TEXT_DOCUMENT_MIN_CHARS = 4000;

function documentExtra(
	name: string,
	mimeType: string,
	segments: DocumentSegment[],
	base64Data?: string
): DatabaseMessageExtraDocumentFile {
	return {
		type: 'documentFile',
		name,
		mimeType,
		segments,
		chars: segments.reduce((n, s) => n + s.text.length, 0),
		pages: segments.reduce((n, s) => Math.max(n, s.page ?? 0), 0),
		...(base64Data ? { base64Data } : {})
	};
}

export interface FileProcessingResult {
	extras: DatabaseMessageExtra[];
	emptyFiles: string[];
}

export async function parseFilesToMessageExtras(
	files: ChatUploadedFile[]
): Promise<FileProcessingResult> {
	const extras: DatabaseMessageExtra[] = [];
	const emptyFiles: string[] = [];

	for (const file of files) {
		if (file.keptDocument) {
			const doc = file.keptDocument;
			extras.push({
				type: 'documentFile',
				name: doc.name,
				mimeType: doc.mime,
				docId: doc.id,
				pages: doc.pages,
				chars: doc.chars,
				library: true
			});
		} else if (getFileTypeCategory(file.type) === FileTypeCategory.IMAGE) {
			if (file.preview) {
				let base64Url = file.preview;

				if (isSvgMimeType(file.type)) {
					try {
						base64Url = await svgBase64UrlToPngDataURL(base64Url);
					} catch (error) {
						console.error('Failed to convert SVG to PNG for database storage:', error);
					}
				} else if (isWebpMimeType(file.type)) {
					try {
						base64Url = await webpBase64UrlToPngDataURL(base64Url);
					} catch (error) {
						console.error('Failed to convert WebP to PNG for database storage:', error);
					}
				}

				extras.push({
					type: 'imageFile',
					name: file.name,
					base64Url
				});
			}
		} else if (getFileTypeCategory(file.type) === FileTypeCategory.AUDIO) {
			// Process audio files (MP3 and WAV)
			try {
				const base64Data = await readFileAsBase64(file.file);

				extras.push({
					type: 'audioFile',
					name: file.name,
					base64Data: base64Data,
					mimeType: file.type
				});
			} catch (error) {
				console.error(`Failed to process audio file ${file.name}:`, error);
			}
		} else if (getFileTypeCategory(file.type) === FileTypeCategory.PDF) {
			try {
				// Always get base64 data for preview functionality
				const base64Data = await readFileAsBase64(file.file);
				const hasVisionSupport = supportsVision();
				let pages: string[] = [];
				try {
					pages = await extractPDFPages(file.file);
				} catch (error) {
					if (!hasVisionSupport) throw error;
				}
				const scans = await readScannedPages(file.name, base64Data, pages);
				pages = scans.pages;
				const segments = pages
					.map((text, i) => ({ page: i + 1, text }))
					.filter((segment) => segment.text.trim());
				if (segments.length) {
					const doc = documentExtra(file.name, MimeTypeApplication.PDF, segments, base64Data);
					// The real page count, even where some pages had no text.
					doc.pages = pages.length;
					extras.push(doc);
				} else if (hasVisionSupport) {
					const images = await convertPDFToImage(file.file);
					extras.push({
						type: 'pdfFile',
						name: file.name,
						content: `PDF file with ${images.length} pages`,
						images,
						processedAsImages: true,
						base64Data
					});
				} else if (!scans.explained) {
					toast.warning(
						`"${file.name}" has no readable text. Choose a model that can read images to use this PDF.`
					);
				}
			} catch (error) {
				console.error(`Failed to process PDF file ${file.name}:`, error);
				toast.error(
					`Could not read "${file.name}". Try another PDF or a model that can read images.`
				);
			}
		} else if (documentKind(file.name, file.type)) {
			const kind = documentKind(file.name, file.type);
			try {
				const segments =
					kind === 'docx'
						? await extractDocxSegments(file.file)
						: await extractSheetSegments(file.file, kind === 'csv');
				if (segments.length === 0) {
					emptyFiles.push(file.name);
				} else {
					extras.push(documentExtra(file.name, file.type || 'application/octet-stream', segments));
				}
			} catch (error) {
				console.error(`Failed to read document ${file.name}:`, error);
				toast.error(
					`Could not read "${file.name}". The file may be damaged or password-protected.`
				);
			}
		} else {
			try {
				const content = await readFileAsText(file.file);

				// Check if file is empty
				if (content.trim() === '') {
					console.warn(`File ${file.name} is empty and will be skipped`);
					emptyFiles.push(file.name);
				} else if (isLikelyTextFile(content) && content.length > TEXT_DOCUMENT_MIN_CHARS) {
					extras.push(documentExtra(file.name, file.type || 'text/plain', [{ text: content }]));
				} else if (isLikelyTextFile(content)) {
					extras.push({
						type: 'textFile',
						name: file.name,
						content: content
					});
				} else {
					console.warn(`File ${file.name} appears to be binary and will be skipped`);
				}
			} catch (error) {
				console.error(`Failed to read file ${file.name}:`, error);
			}
		}
	}

	return { extras, emptyFiles };
}
