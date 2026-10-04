import { convertPDFToImage, convertPDFToText } from './pdf-processing';
import { isSvgMimeType, svgBase64UrlToPngDataURL } from './svg-to-png';
import { isWebpMimeType, webpBase64UrlToPngDataURL } from './webp-to-png';
import { FileTypeCategory } from '$lib/enums/files';
import { supportsVision } from '$lib/stores/server.svelte';
import { getFileTypeCategory } from '$lib/utils/file-type';
import { readFileAsText, isLikelyTextFile } from './text-files';
import { toast } from 'svelte-sonner';

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
		if (getFileTypeCategory(file.type) === FileTypeCategory.IMAGE) {
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
				let content = '';
				try {
					content = await convertPDFToText(file.file);
				} catch (error) {
					if (!hasVisionSupport) throw error;
				}
				if (content.trim()) {
					extras.push({
						type: 'pdfFile',
						name: file.name,
						content,
						processedAsImages: false,
						base64Data
					});
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
				} else {
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
		} else {
			try {
				const content = await readFileAsText(file.file);

				// Check if file is empty
				if (content.trim() === '') {
					console.warn(`File ${file.name} is empty and will be skipped`);
					emptyFiles.push(file.name);
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
