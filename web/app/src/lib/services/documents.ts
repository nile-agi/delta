import { getModelApiBaseUrl } from '$lib/utils/model-api-url';
import type { DocumentSegment } from '$lib/utils/document-extraction';

// Documents live in the engine next to the agent, so reuse the model API's base URL resolution.
function apiUrl(path: string): string {
	return `${getModelApiBaseUrl()}${path}`;
}

export interface IndexedDocument {
	id: string;
	name: string;
	mime: string;
	scope: 'conversation' | 'library';
	pages: number;
	chars: number;
	chunks: number;
	created_at: string;
}

async function ok(res: Response): Promise<Response> {
	if (!res.ok) {
		const data = await res.json().catch(() => ({}));
		throw new Error(data.error || `Document request failed (${res.status})`);
	}
	return res;
}

export const documentsService = {
	/** Chunks and indexes extracted text for one conversation. */
	async upload(input: {
		name: string;
		mime: string;
		conversationId: string;
		segments: DocumentSegment[];
	}): Promise<IndexedDocument> {
		const res = await fetch(apiUrl('/api/documents'), {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify({
				name: input.name,
				mime: input.mime,
				conversation_id: input.conversationId,
				segments: input.segments
			})
		});
		return (await (await ok(res)).json()).document;
	},

	/** Keep a document for all chats, or stop keeping it. */
	async setLibrary(id: string, library: boolean): Promise<void> {
		await ok(
			await fetch(apiUrl(`/api/documents/${encodeURIComponent(id)}`), {
				method: 'PUT',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify({ library })
			})
		);
	},

	/** Make an indexed document part of another conversation. */
	async attach(id: string, conversationId: string): Promise<void> {
		await ok(
			await fetch(apiUrl(`/api/documents/${encodeURIComponent(id)}`), {
				method: 'PUT',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify({ attach: conversationId })
			})
		);
	},

	/** Documents kept for all chats. */
	async library(): Promise<IndexedDocument[]> {
		const res = await ok(await fetch(apiUrl('/api/documents?library=1')));
		return (await res.json()).documents ?? [];
	},

	/** Forget a deleted conversation's files. Best effort: the chat is gone either way. */
	async removeConversation(conversationId: string): Promise<void> {
		try {
			await fetch(apiUrl(`/api/documents?conversation_id=${encodeURIComponent(conversationId)}`), {
				method: 'DELETE'
			});
		} catch (error) {
			console.warn('Could not remove the conversation documents:', error);
		}
	}
};
