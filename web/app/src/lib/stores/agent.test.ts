import { describe, expect, it } from 'vitest';

const { agentStore } = await import('./agent.svelte');

const event = (name: string, data: Record<string, unknown>) =>
	({ object: 'delta.agent.event', event: name, data }) as unknown as AgentEvent;

describe('agent store', () => {
	it('keeps the run transcript through finish() so it can be persisted', () => {
		agentStore.begin('m1');
		agentStore.handleEvent('m1', event('tool_start', { call_id: 'c1', name: 'read_file', arguments: {}, risk: 'caution' }));
		agentStore.handleEvent('m1', event('tool_result', { call_id: 'c1', name: 'read_file', success: true }));
		agentStore.handleEvent(
			'm1',
			event('run_summary', {
				tool_calls: 1,
				iterations: 2,
				stop_reason: 'stop',
				tools: [],
				transcript: [
					{ role: 'assistant', content: '', tool_calls: [{ id: 'c1' }] },
					{ role: 'tool', content: '{}', tool_call_id: 'c1', name: 'read_file' },
					{ role: 'assistant', content: 'done' }
				]
			})
		);

		const activity = agentStore.finish('m1');
		expect(activity?.transcript?.length).toBe(3);
	});

	it('matches a result to the step with the same call id, not just the same name', () => {
		agentStore.begin('m2');
		agentStore.handleEvent('m2', event('tool_start', { call_id: 'a', name: 'read_file', arguments: {}, risk: 'caution' }));
		agentStore.handleEvent('m2', event('tool_start', { call_id: 'b', name: 'read_file', arguments: {}, risk: 'caution' }));
		agentStore.handleEvent('m2', event('tool_result', { call_id: 'a', name: 'read_file', success: false, error: 'nope' }));

		const steps = agentStore.activityFor('m2')?.steps ?? [];
		expect(steps.map((s) => s.status)).toEqual(['failed', 'running']);
	});
});
