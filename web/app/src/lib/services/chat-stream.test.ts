import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

vi.mock('$lib/stores/settings.svelte', () => ({ config: () => ({}) }));
vi.mock('$lib/stores/models.svelte', () => ({
	selectedModelName: () => 'test-model',
	selectedModelOption: () => null
}));
vi.mock('$lib/utils/model-api-url', () => ({ getModelApiBaseUrl: () => 'http://agent.test' }));
vi.mock('$lib/utils/server-base-url', () => ({ getServerBaseUrl: () => 'http://plain.test' }));
vi.mock('./slots', () => ({ slotsService: { setActiveConversation: vi.fn() } }));
const desktop = vi.hoisted(() => ({ invoke: vi.fn() }));
vi.mock('@tauri-apps/api/core', () => ({
	invoke: desktop.invoke,
	Channel: class {
		onmessage = (_line: string) => {};
	}
}));

const { ChatService } = await import('./chat');

class TestXHR {
	static requests: TestXHR[] = [];
	responseText = '';
	status = 200;
	onprogress = () => {};
	onload = () => {};
	onabort = () => {};
	open() {}
	setRequestHeader() {}
	send() {
		TestXHR.requests.push(this);
	}
	abort() {
		this.onabort();
	}
	push(value: unknown) {
		this.responseText += `data: ${typeof value === 'string' ? value : JSON.stringify(value)}\r\n\r\n`;
		this.onprogress();
	}
}

beforeEach(() => {
	TestXHR.requests = [];
	vi.stubGlobal('XMLHttpRequest', TestXHR);
});
afterEach(() => vi.unstubAllGlobals());

const content = (text: string) => ({
	object: 'chat.completion.chunk',
	choices: [{ delta: { content: text } }]
});

describe('stream lifecycle', () => {
	it('keeps tool transcripts and completes on DONE while the connection is still open', async () => {
		const onAgentEvent = vi.fn();
		const onComplete = vi.fn();
		const pending = new ChatService().sendMessage([{ role: 'user', content: 'hello' }], {
			stream: true,
			useTools: true,
			onAgentEvent,
			onComplete
		});
		const xhr = TestXHR.requests[0];
		const close = vi.spyOn(xhr, 'abort');
		xhr.push({
			object: 'delta.agent.event',
			event: 'run_summary',
			data: {
				transcript: [{ role: 'assistant', content: 'Hello' }]
			}
		});
		xhr.push(content('Hello'));
		xhr.push('[DONE]');
		await vi.waitFor(() =>
			expect(onComplete).toHaveBeenCalledWith('Hello', undefined, undefined, undefined)
		);
		await pending;
		expect(close).toHaveBeenCalledOnce();
		expect(onAgentEvent).toHaveBeenCalledWith(expect.objectContaining({ event: 'run_summary' }));
		xhr.onload();
		expect(onComplete).toHaveBeenCalledTimes(1);
	});

	it('does not let cleanup from a replaced request remove the new request’s Stop handler', async () => {
		const service = new ChatService();
		const first = service.sendMessage(
			[{ role: 'user', content: 'first' }],
			{ stream: true },
			'same-chat'
		);
		const second = service.sendMessage(
			[{ role: 'user', content: 'second' }],
			{ stream: true },
			'same-chat'
		);
		const abort = vi.spyOn(TestXHR.requests[1], 'abort');
		await first;
		service.abort('same-chat');
		expect(abort).toHaveBeenCalledOnce();
		await second;
	});

	it('reports a broken stream instead of completing a partial answer', async () => {
		const onComplete = vi.fn();
		const pending = new ChatService().sendMessage([{ role: 'user', content: 'hello' }], {
			stream: true,
			onComplete
		});
		const rejected = expect(pending).rejects.toThrow(/stream.*end|incomplete/i);
		TestXHR.requests[0].push(content('partial'));
		TestXHR.requests[0].onload();
		await rejected;
		expect(onComplete).not.toHaveBeenCalled();
	});

	it('awaits persistence and reports a failure in the completion callback', async () => {
		const onError = vi.fn();
		vi.stubGlobal(
			'fetch',
			vi
				.fn()
				.mockResolvedValue(
					new Response(JSON.stringify({ choices: [{ message: { content: 'Hello' } }] }))
				)
		);
		const pending = new ChatService().sendMessage([{ role: 'user', content: 'hello' }], {
			onComplete: async () => {
				throw new Error('Could not save reply');
			},
			onError
		});
		await expect(pending).rejects.toThrow('Could not save reply');
		expect(onError).toHaveBeenCalledWith(
			expect.objectContaining({ message: 'Could not save reply' })
		);
	});

	it('reports failure while saving a streamed reply so loading can clear', async () => {
		const onError = vi.fn();
		const pending = new ChatService().sendMessage([{ role: 'user', content: 'can you code calc in ruby' }], {
			stream: true,
			onComplete: async () => { throw new Error('Could not save reply'); },
			onError
		});
		const rejected = expect(pending).rejects.toThrow('Could not save reply');
		TestXHR.requests[0].push(content('```ruby\ndef calculate_result(data)\n```'));
		TestXHR.requests[0].push('[DONE]');
		await rejected;
		expect(onError).toHaveBeenCalledWith(expect.objectContaining({ message: 'Could not save reply' }));
	});

	it('does not retry a desktop response after it has already streamed content', async () => {
		vi.resetModules();
		vi.stubGlobal('window', { __TAURI_INTERNALS__: {} });
		const { ChatService: DesktopChat } = await import('./chat');
		desktop.invoke.mockImplementation(async (_command, args) => {
			args.onChunk.onmessage(`data: ${JSON.stringify(content('partial'))}`);
			throw new Error('Response stream failed: connection lost');
		});
		await expect(
			new DesktopChat().sendMessage([{ role: 'user', content: 'hello' }], {
				stream: true
			})
		).rejects.toThrow('connection lost');
		expect(TestXHR.requests).toHaveLength(0);
	});

	it('reports desktop agent errors and uses a fresh native stream id for each turn', async () => {
		vi.resetModules();
		vi.stubGlobal('window', { __TAURI_INTERNALS__: {} });
		const { ChatService: DesktopChat } = await import('./chat');
		const ids: string[] = [];
		desktop.invoke.mockImplementation(async (_command, args) => {
			ids.push(args.streamId);
			args.onChunk.onmessage(
				'data: {"object":"delta.agent.event","event":"error","data":{"message":"worker failed"}}'
			);
			args.onChunk.onmessage('data: [DONE]');
			return 200;
		});
		const service = new DesktopChat();
		for (let turn = 0; turn < 2; turn++) {
			await expect(
				service.sendMessage(
					[{ role: 'user', content: 'hello' }],
					{
						stream: true,
						useTools: true
					},
					'same-chat'
				)
			).rejects.toThrow('worker failed');
		}
		expect(new Set(ids).size).toBe(2);
	});
});
