import { afterEach, describe, expect, it, vi } from 'vitest';

const configState = vi.hoisted(() => ({
	current: { systemMessage: 'Use plain words.', userName: 'Asha', replyStyle: 'concise' } as Record<
		string,
		string
	>
}));
vi.mock('$lib/stores/settings.svelte', () => ({
	config: () => configState.current
}));
vi.mock('$lib/stores/models.svelte', () => ({
	selectedModelName: () => 'test-model',
	selectedModelOption: () => null
}));
vi.mock('$lib/utils/server-base-url', () => ({ getServerBaseUrl: () => 'http://plain.test' }));
vi.mock('$lib/utils/model-api-url', () => ({ getModelApiBaseUrl: () => 'http://agent.test' }));
vi.mock('./slots', () => ({ slotsService: { setActiveConversation: vi.fn() } }));

const { ChatService } = await import('./chat');
afterEach(() => {
	vi.unstubAllGlobals();
	configState.current = {
		systemMessage: 'Use plain words.',
		userName: 'Asha',
		replyStyle: 'concise'
	};
});

describe('chat request guidance and history', () => {
	for (const useTools of [false, true]) {
		it(`rejects invalid DRY lookback before sending with tools=${useTools}`, async () => {
			const fetch = vi.fn();
			vi.stubGlobal('fetch', fetch);
			const onError = vi.fn();
			await expect(
				new ChatService().sendMessage([{ role: 'user', content: 'Hello' }], {
					useTools,
					dry_penalty_last_n: -1,
					onError
				})
			).rejects.toThrow(/lookback/i);
			expect(fetch).not.toHaveBeenCalled();
			expect(onError).toHaveBeenCalledOnce();
		});
	}
	it('rejects invalid custom sampling aliases before sending', async () => {
		const fetch = vi.fn();
		vi.stubGlobal('fetch', fetch);
		await expect(
			new ChatService().sendMessage([{ role: 'user', content: 'Hello' }], {
				custom: '{"typical_p":2}'
			})
		).rejects.toThrow(/probability/i);
		expect(fetch).not.toHaveBeenCalled();
	});
	it('retains caller instructions on the agent path with empty settings', async () => {
		configState.current = {};
		const fetch = vi
			.fn()
			.mockResolvedValue(
				new Response(JSON.stringify({ choices: [{ message: { content: 'Done.' } }] }))
			);
		vi.stubGlobal('fetch', fetch);
		await new ChatService().sendMessage(
			[
				{ role: 'system', content: 'Use supplied facts.' },
				{ role: 'user', content: 'Hello' }
			],
			{ stream: false, useTools: true }
		);
		const request = JSON.parse(fetch.mock.calls[0][1].body);
		expect(request.messages).toEqual([
			{ role: 'system', content: 'Use supplied facts.' },
			{ role: 'user', content: 'Hello' }
		]);
	});
	it('refreshes generated preferences without dropping caller lines beside the clock', async () => {
		const fetch = vi
			.fn()
			.mockResolvedValue(
				new Response(JSON.stringify({ choices: [{ message: { content: 'Done.' } }] }))
			);
		vi.stubGlobal('fetch', fetch);
		await new ChatService().sendMessage(
			[
				{
					role: 'system',
					content:
						"Today is Sunday, 4 October 2026. The time is 10:00 (Africa/Dar_es_Salaam).\nAlways answer in Kiswahili.\n\nThe user's name is Musa. Address them by name naturally, not in every sentence.\n\nGive thorough answers with the reasoning and relevant context included."
				},
				{ role: 'user', content: 'Hello' }
			],
			{ stream: false, useTools: false }
		);
		const prompt = JSON.parse(fetch.mock.calls[0][1].body).messages[0].content;
		expect(prompt).toContain('Always answer in Kiswahili.');
		expect(prompt).toContain('Asha');
		expect(prompt).toContain('Keep answers short');
		expect(prompt).not.toContain('Musa');
		expect(prompt).not.toContain('Give thorough answers');
		expect(prompt.match(/Today is /g)).toHaveLength(1);
	});
	for (const useTools of [false, true]) {
		it(`preserves the active history and settings with tools=${useTools}`, async () => {
			const fetch = vi
				.fn()
				.mockResolvedValue(
					new Response(JSON.stringify({ choices: [{ message: { content: 'Done.' } }] }))
				);
			vi.stubGlobal('fetch', fetch);
			const history = [
				{ role: 'system' as const, content: 'Preserve the caller instruction.' },
				{ role: 'user' as const, content: 'The delivery is Thursday.' },
				{ role: 'assistant' as const, content: 'Noted.' },
				{ role: 'user' as const, content: 'Correction: Friday. Translate that into Kiswahili.' }
			];
			await new ChatService().sendMessage(history, { stream: false, useTools });
			const request = JSON.parse(fetch.mock.calls[0][1].body);
			expect(request.messages.slice(1)).toEqual(history.slice(1));
			expect(request.messages.filter((m: { role: string }) => m.role === 'system')).toHaveLength(1);
			expect(request.messages[0].content).toContain('Use plain words.');
			expect(request.messages[0].content).toContain('Asha');
			expect(request.messages[0].content).toContain('Preserve the caller instruction.');
			expect(history).toHaveLength(4);
		});
	}
	it('accepts spaces around sampler names and sends clean names', async () => {
		const fetch = vi
			.fn()
			.mockResolvedValue(
				new Response(JSON.stringify({ choices: [{ message: { content: 'Done.' } }] }))
			);
		vi.stubGlobal('fetch', fetch);
		await new ChatService().sendMessage([{ role: 'user', content: 'Hello' }], {
			stream: false,
			samplers: 'top_k; top_p; temperature'
		});
		expect(JSON.parse(fetch.mock.calls[0][1].body).samplers).toEqual([
			'top_k',
			'top_p',
			'temperature'
		]);
	});
});
