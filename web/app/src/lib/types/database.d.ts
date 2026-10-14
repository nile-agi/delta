import type { AgentActivity, QuickAddSuggestion } from './agent';

import type { ChatMessageTimings } from './chat';

export interface DatabaseConversation {
	currNode: string | null;
	id: string;
	lastModified: number;
	name: string;
}

export interface DatabaseMessageExtraAudioFile {
	type: 'audioFile';
	name: string;
	base64Data: string;
	mimeType: string;
}

export interface DatabaseMessageExtraImageFile {
	type: 'imageFile';
	name: string;
	base64Url: string;
}

export interface DatabaseMessageExtraTextFile {
	type: 'textFile';
	name: string;
	content: string;
}

export interface DatabaseMessageExtraPdfFile {
	type: 'pdfFile';
	name: string;
	content: string; // Text content extracted from PDF
	images?: string[]; // Optional: PDF pages as base64 images
	processedAsImages: boolean; // Whether PDF was processed as images
	base64Data?: string; // Original bytes, for the preview and the citation viewer
}

/**
 * A PDF, Word, spreadsheet or long text file. Small ones are sent whole, like a text file. Large
 * ones are indexed by the engine (`docId`), and the model is given only the passages that match
 * each question.
 */
export interface DatabaseMessageExtraDocumentFile {
	type: 'documentFile';
	name: string;
	mimeType: string;
	/** Engine document id once indexed. Absent means `content` is sent whole. */
	docId?: string;
	/** Whole text, kept only while the file is small enough to send whole. */
	content?: string;
	pages: number;
	chars: number;
	/** Kept for all chats (the shared library). */
	library?: boolean;
	/** Original PDF bytes, for the preview dialog. */
	base64Data?: string;
	/** Extracted text awaiting indexing; dropped before the message is saved. */
	segments?: import('$lib/utils/document-extraction').DocumentSegment[];
}

/**
 * Legacy format from old webui - pasted content was stored as "context" type
 * @deprecated Use DatabaseMessageExtraTextFile instead
 */
export interface DatabaseMessageExtraLegacyContext {
	type: 'context';
	name: string;
	content: string;
}

export type DatabaseMessageExtra =
	| DatabaseMessageExtraImageFile
	| DatabaseMessageExtraTextFile
	| DatabaseMessageExtraAudioFile
	| DatabaseMessageExtraPdfFile
	| DatabaseMessageExtraDocumentFile
	| DatabaseMessageExtraLegacyContext;

/** Harmony-compatible tool call (delta.tool_calls) */
export interface DatabaseMessageToolCall {
	name?: string;
	arguments?: string;
	[id: string]: unknown;
}

export interface DatabaseMessage {
	id: string;
	convId: string;
	type: ChatMessageType;
	timestamp: number;
	role: ChatRole;
	content: string;
	parent: string;
	thinking: string;
	children: string[];
	extra?: DatabaseMessageExtra[];
	timings?: ChatMessageTimings;
	model?: string;
	tool_calls?: DatabaseMessageToolCall[];
	/** What the Delta harness did while producing this message: tool steps, notices, compaction. */
	agent_activity?: AgentActivity;
	/** A calendar item suggested from this user message; see QuickAddSuggestion. */
	quick_add?: QuickAddSuggestion;
}

/**
 * Represents a single conversation with its associated messages,
 * typically used for import/export operations.
 */
export type ExportedConversation = {
	conv: DatabaseConversation;
	messages: DatabaseMessage[];
};

/**
 * Type representing one or more exported conversations.
 * Can be a single conversation object or an array of them.
 */
export type ExportedConversations = ExportedConversation | ExportedConversation[];
