import { MimeTypeApplication } from '$lib/enums/files';
import { documentsService } from '$lib/services/documents';
import { segmentsText } from './document-extraction';

/**
 * Up to this many characters a file is sent whole; past it the engine indexes it and the model is
 * given the passages that match each question. A fifth of the context window, never more than
 * about 6,000 tokens, at roughly 3.5 characters a token.
 */
export function inlineLimitChars(contextSize?: number | null): number {
	const ctx = contextSize && contextSize > 0 ? contextSize : 4096;
	return Math.round(Math.min(6000, ctx * 0.2) * 3.5);
}

function inline(doc: DatabaseMessageExtraDocumentFile, content: string): DatabaseMessageExtra {
	if (doc.mimeType === MimeTypeApplication.PDF) {
		return {
			type: 'pdfFile',
			name: doc.name,
			content,
			processedAsImages: false,
			...(doc.base64Data ? { base64Data: doc.base64Data } : {})
		} as DatabaseMessageExtraPdfFile;
	}
	return { type: 'textFile', name: doc.name, content };
}

/**
 * Resolves freshly extracted documents once the conversation exists: small ones become ordinary
 * inline attachments, large ones are indexed and kept as a reference. If indexing fails (no
 * engine, say), the file is sent whole as before, so attaching never just stops working.
 */
export async function indexDocumentExtras(
	extras: DatabaseMessageExtra[],
	conversationId: string,
	contextSize?: number | null
): Promise<DatabaseMessageExtra[]> {
	const limit = inlineLimitChars(contextSize);
	const out: DatabaseMessageExtra[] = [];
	for (const extra of extras) {
		if (extra.type !== 'documentFile' || !extra.segments) {
			out.push(extra);
			continue;
		}
		const { segments, ...doc } = extra;
		if (doc.chars > limit) {
			try {
				const indexed = await documentsService.upload({
					name: doc.name,
					mime: doc.mimeType,
					conversationId,
					segments
				});
				out.push({ ...doc, docId: indexed.id, pages: indexed.pages || doc.pages });
				continue;
			} catch (error) {
				console.warn(`Could not index "${doc.name}", sending it whole:`, error);
			}
		}
		out.push(inline(doc, segmentsText(segments)));
	}
	return out;
}
