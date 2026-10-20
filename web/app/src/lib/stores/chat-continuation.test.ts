import { afterEach, beforeEach, expect, it, vi } from 'vitest';

vi.mock('$app/environment', () => ({ browser: false }));
vi.mock('$app/navigation', () => ({ goto: vi.fn() }));
vi.mock('$app/paths', () => ({ resolve: (path: string) => path }));
vi.mock('$lib/stores/settings.svelte', () => ({ config: () => ({}) }));
vi.mock('$lib/stores/server.svelte', () => ({ serverStore: { serverProps: null } }));
vi.mock('$lib/stores/models.svelte', () => ({
	agentToolsActive: () => false,
	selectedModelName: () => 'test-model',
	modelLoadedOnServer: vi.fn(() => true),
	requestModelSelection: vi.fn()
}));
vi.mock('$lib/utils/index-document-extras', () => ({ indexDocumentExtras: vi.fn() }));
vi.mock('$lib/services', () => ({
	chatService: { sendMessage: vi.fn() },
	slotsService: {
		startStreaming: vi.fn(),
		stopStreaming: vi.fn(),
		setActiveConversation: vi.fn(),
		clearConversationState: vi.fn()
	}
}));
vi.mock('$lib/stores/database', () => ({
	DatabaseStore: { getConversationPath: vi.fn(), deleteMessage: vi.fn() }
}));

const { chatStore } = await import('./chat.svelte');
const { DatabaseStore } = await import('./database');
const { chatService } = await import('$lib/services');
const { modelLoadedOnServer, requestModelSelection } = await import('./models.svelte');

afterEach(() => vi.restoreAllMocks());
beforeEach(() => {
	vi.clearAllMocks();
	vi.mocked(modelLoadedOnServer).mockReturnValue(true);
	chatStore.errorDialogState = null;
});

it.each(['worker failed', "model 'default' not found"])(
	'preserves a saved reply when its continuation fails: %s',
	async (message) => {
		const original = {
			id: 'reply',
			convId: 'conversation',
			role: 'assistant',
			content: 'Original answer',
			timestamp: 1,
			parent: '-1',
			thinking: '',
			type: 'text'
		} as DatabaseMessage;
		chatStore.activeConversation = {
			id: 'conversation',
			currNode: 'reply'
		} as DatabaseConversation;
		chatStore.activeMessages = [{ ...original }];
		vi.mocked(DatabaseStore.getConversationPath).mockResolvedValue([original]);
		vi.spyOn(console, 'error').mockImplementation(() => {});
		vi.mocked(chatService.sendMessage).mockImplementation(async (_messages, options) => {
			options?.onError?.(new Error(message));
		});

		await chatStore.continueMessage('reply');
		expect(chatService.sendMessage).toHaveBeenCalledOnce();
		expect(vi.mocked(chatService.sendMessage).mock.calls[0][1]?.continueFinalMessage).toBe(true);
		expect(vi.mocked(chatService.sendMessage).mock.calls[0][1]?.useTools).not.toBe(true);
		expect(DatabaseStore.deleteMessage).not.toHaveBeenCalled();
		expect(chatStore.activeMessages.map((m) => m.content)).toEqual(['Original answer']);
		expect(chatStore.activeConversation?.currNode).toBe('reply');
		expect(chatStore.isLoading).toBe(false);
		if (message.includes('not found')) {
			expect(requestModelSelection).toHaveBeenCalledOnce();
			expect(chatStore.errorDialogState).toBeNull();
		}
	}
);

it('opens model selection before Continue when no model is loaded', async () => {
	chatStore.activeConversation = { id: 'conversation', currNode: 'reply' } as DatabaseConversation;
	chatStore.isLoading = false;
	vi.mocked(modelLoadedOnServer).mockReturnValue(false);
	await chatStore.continueMessage('reply');
	expect(requestModelSelection).toHaveBeenCalledOnce();
	expect(chatService.sendMessage).not.toHaveBeenCalled();
	expect(DatabaseStore.getConversationPath).not.toHaveBeenCalled();
});
