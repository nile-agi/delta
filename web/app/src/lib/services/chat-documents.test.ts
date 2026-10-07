import { afterEach, describe, expect, it, vi } from 'vitest';

vi.mock('$lib/stores/settings.svelte', () => ({ config: () => ({}) }));
vi.mock('$lib/stores/models.svelte', () => ({
	selectedModelName: () => 'test-model',
	selectedModelOption: () => null
}));
vi.mock('$lib/utils/server-base-url', () => ({ getServerBaseUrl: () => 'http://plain.test' }));
vi.mock('$lib/utils/model-api-url', () => ({ getModelApiBaseUrl: () => 'http://agent.test' }));
vi.mock('./slots', () => ({ slotsService: { setActiveConversation: vi.fn() } }));

const { ChatService } = await import('./chat');
afterEach(() => vi.unstubAllGlobals());

function userMessage(extra: DatabaseMessageExtra[]): DatabaseMessage {
	return {
		id: 'm1',
		convId: 'conv-7',
		type: 'text',
		timestamp: 1,
		role: 'user',
		content: 'When does the pump warranty expire?',
		parent: '-1',
		thinking: '',
		children: [],
		extra
	} as unknown as DatabaseMessage;
}

async function send(extra: DatabaseMessageExtra[]) {
	const fetch = vi
		.fn()
		.mockResolvedValue(new Response(JSON.stringify({ choices: [{ message: { content: 'Ok.' } }] })));
	vi.stubGlobal('fetch', fetch);
	await new ChatService().sendMessage([userMessage(extra)], { useTools: false }, 'conv-7');
	return { url: fetch.mock.calls[0][0] as string, body: JSON.parse(fetch.mock.calls[0][1].body) };
}

describe('indexed documents in chat requests', () => {
	it('sends a stub instead of the file and lets the agent server add passages', async () => {
		const { url, body } = await send([
			{
				type: 'documentFile',
				name: 'manual.pdf',
				mimeType: 'application/pdf',
				docId: 'doc-1',
				pages: 120,
				chars: 400000
			}
		]);
		expect(url).toBe('http://agent.test/v1/chat/completions');
		expect(body.conversation_id).toBe('conv-7');
		expect(body.use_documents).toBe(true);
		expect(body.use_tools).toBe(false);
		const text = JSON.stringify(body.messages);
		expect(text).toContain('Attached file: manual.pdf (120 pages');
		expect(text.length).toBeLessThan(2000);
	});

	it('leaves a chat without indexed files on the plain server', async () => {
		const { url, body } = await send([{ type: 'textFile', name: 'a.txt', content: 'hello' }]);
		expect(url).toBe('http://plain.test/v1/chat/completions');
		expect(body.use_documents).toBeUndefined();
		expect(body.conversation_id).toBeUndefined();
	});
});
