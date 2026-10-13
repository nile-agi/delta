/**
 * Where Back from Settings goes: the chat that was open, or home when none was. Home clears the
 * active chat on mount, so sending everyone there used to start a new chat on every visit.
 * The leading `?` drops Settings' own `?section=` query.
 */
export function settingsReturnPath(conversationId?: string | null): string {
	return conversationId ? `?#/chat/${encodeURIComponent(conversationId)}` : '?#/';
}
