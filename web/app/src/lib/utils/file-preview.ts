import { MimeTypeDocument } from '$lib/enums/files';

/**
 * Formats file size in bytes to human readable format
 * @param bytes - File size in bytes
 * @returns Formatted file size string
 */
export function formatFileSize(bytes: number): string {
	if (bytes === 0) return '0 Bytes';

	const k = 1024;
	const sizes = ['Bytes', 'KB', 'MB', 'GB'];
	const i = Math.floor(Math.log(bytes) / Math.log(k));

	return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

/**
 * Gets a display label for a file type
 * @param fileType - The file type/mime type
 * @returns Formatted file type label
 */
const SHORT_LABELS: Record<string, string> = {
	[MimeTypeDocument.DOCX]: 'DOCX',
	[MimeTypeDocument.XLSX]: 'XLSX',
	[MimeTypeDocument.XLS]: 'XLS',
	[MimeTypeDocument.ODS]: 'ODS',
	'application/octet-stream': 'FILE'
};

export function getFileTypeLabel(fileType: string): string {
	return SHORT_LABELS[fileType] ?? (fileType.split('/').pop()?.toUpperCase() || 'FILE');
}

/**
 * Truncates text content for preview display
 * @param content - The text content to truncate
 * @returns Truncated content with ellipsis if needed
 */
export function getPreviewText(content: string): string {
	return content.length > 150 ? content.substring(0, 150) + '...' : content;
}
