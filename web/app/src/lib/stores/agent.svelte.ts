import { SvelteMap } from 'svelte/reactivity';
import { getModelApiBaseUrl } from '$lib/utils/model-api-url';

/**
 * AgentStore - what the harness did during a turn, and the approval it is waiting on.
 *
 * The engine streams `delta.agent.event` frames alongside the assistant's text. This store turns
 * that sequence into something renderable: a list of tool steps per message, any notices about
 * context being compacted, and the single approval request that can be outstanding at a time.
 *
 * There is only ever one pending approval because the run blocks on the server until it is
 * answered -- the harness cannot reach a second tool call while the first is waiting.
 */
class AgentStore {
	/** Tool activity keyed by the assistant message it belongs to. */
	activities = new SvelteMap<string, AgentActivity>();

	/** The approval the user is being asked about, with the message it came from. */
	pendingApproval = $state<{ messageId: string; request: AgentApprovalRequiredData } | null>(null);

	/** True while the answer is in flight, so the buttons can disable themselves. */
	isRespondingToApproval = $state(false);

	private emptyActivity(): AgentActivity {
		return { steps: [], notices: [] };
	}

	activityFor(messageId: string): AgentActivity | undefined {
		return this.activities.get(messageId);
	}

	/** Clears anything left from a previous run of the same message (a regeneration). */
	begin(messageId: string): void {
		this.activities.delete(messageId);
		if (this.pendingApproval?.messageId === messageId) {
			this.pendingApproval = null;
		}
	}

	/** Restores persisted activity when an old conversation is opened. */
	hydrate(messageId: string, activity: AgentActivity | undefined): void {
		if (!activity || (!activity.steps?.length && !activity.notices?.length)) return;
		this.activities.set(messageId, {
			steps: activity.steps ?? [],
			notices: activity.notices ?? [],
			compaction: activity.compaction,
			stopReason: activity.stopReason,
			iterations: activity.iterations
		});
	}

	private mutate(messageId: string, fn: (activity: AgentActivity) => void): void {
		const current = this.activities.get(messageId) ?? this.emptyActivity();
		const next: AgentActivity = {
			steps: [...current.steps],
			notices: [...current.notices],
			compaction: current.compaction,
			stopReason: current.stopReason,
			iterations: current.iterations
		};
		fn(next);
		this.activities.set(messageId, next);
	}

	/**
	 * Finds the step a result belongs to: the most recent one still open for that tool name.
	 * Matching by name from the end is enough because the harness runs tool calls one at a time.
	 */
	private findOpenStep(activity: AgentActivity, name: string): AgentActivityStep | undefined {
		for (let i = activity.steps.length - 1; i >= 0; i--) {
			const step = activity.steps[i];
			if (
				step.name === name &&
				(step.status === 'running' || step.status === 'awaiting-approval')
			) {
				return step;
			}
		}
		return undefined;
	}

	handleEvent(messageId: string, event: AgentEvent): void {
		switch (event.event) {
			case 'tool_start': {
				const data = event.data as AgentToolStartData;
				this.mutate(messageId, (activity) => {
					activity.steps.push({
						id: `${messageId}-${activity.steps.length}-${data.name}`,
						name: data.name,
						arguments: data.arguments ?? {},
						risk: data.risk ?? 'safe',
						status: 'running',
						startedAt: Date.now()
					});
				});
				break;
			}

			case 'approval_required': {
				const data = event.data as AgentApprovalRequiredData;
				this.mutate(messageId, (activity) => {
					const step = this.findOpenStep(activity, data.name);
					if (step) {
						step.status = 'awaiting-approval';
						step.approvalId = data.id;
					}
				});
				this.pendingApproval = { messageId, request: data };
				break;
			}

			case 'approval_resolved': {
				const data = event.data as { id: string; decision: string };
				if (this.pendingApproval?.request.id === data.id) {
					this.pendingApproval = null;
				}
				this.isRespondingToApproval = false;
				// A step denied here gets its final status from the tool_result that follows, so
				// only the timeout case needs a note of its own.
				if (data.decision === 'timeout') {
					this.mutate(messageId, (activity) => {
						activity.notices.push('No answer to the approval request, so the action was skipped.');
					});
				}
				break;
			}

			case 'tool_result': {
				const data = event.data as AgentToolResultData;
				this.mutate(messageId, (activity) => {
					const step = this.findOpenStep(activity, data.name);
					if (!step) return;
					step.status = data.success
						? 'success'
						: step.status === 'awaiting-approval'
							? 'denied'
							: 'failed';
					step.summary = data.summary;
					step.error = data.error || undefined;
					step.endedAt = Date.now();
				});
				break;
			}

			case 'compaction': {
				const data = event.data as AgentCompactionData;
				this.mutate(messageId, (activity) => {
					activity.compaction = data;
				});
				break;
			}

			case 'status': {
				const data = event.data as AgentStatusData;
				if (data.message) {
					this.mutate(messageId, (activity) => {
						activity.notices.push(data.message);
					});
				}
				break;
			}

			case 'error': {
				const data = event.data as { message?: string };
				if (data.message) {
					this.mutate(messageId, (activity) => {
						activity.notices.push(data.message as string);
					});
				}
				break;
			}

			case 'run_summary': {
				const data = event.data as AgentRunSummaryData;
				this.mutate(messageId, (activity) => {
					activity.stopReason = data.stop_reason;
					activity.iterations = data.iterations;
				});
				break;
			}
		}
	}

	/**
	 * Closes out a run: any step still marked running was cut short by an abort or a stream error.
	 * Returns the activity so the caller can persist it with the message.
	 */
	finish(messageId: string): AgentActivity | undefined {
		if (this.pendingApproval?.messageId === messageId) {
			this.pendingApproval = null;
			this.isRespondingToApproval = false;
		}
		const activity = this.activities.get(messageId);
		if (!activity) return undefined;
		this.mutate(messageId, (next) => {
			for (const step of next.steps) {
				if (step.status === 'running' || step.status === 'awaiting-approval') {
					step.status = 'failed';
					step.error = step.error ?? 'Interrupted';
					step.endedAt = Date.now();
				}
			}
		});
		return this.activities.get(messageId);
	}

	/**
	 * Answers the pending approval. `always` and `never` are remembered by the engine across
	 * conversations; `allow` and `deny` apply to this call only.
	 */
	async respond(decision: AgentApprovalDecision): Promise<void> {
		const pending = this.pendingApproval;
		if (!pending || this.isRespondingToApproval) return;

		this.isRespondingToApproval = true;
		try {
			const response = await fetch(`${getModelApiBaseUrl()}/v1/agent/approve`, {
				method: 'POST',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify({ id: pending.request.id, decision })
			});
			if (!response.ok) {
				throw new Error(`Approval failed with status ${response.status}`);
			}
			// The engine confirms with approval_resolved, which clears the prompt. Clearing it here
			// too keeps the UI responsive if that frame is slow to arrive.
			this.pendingApproval = null;
		} catch (error) {
			console.error('[Delta] Could not send the approval decision:', error);
			this.mutate(pending.messageId, (activity) => {
				activity.notices.push('Could not reach the engine to answer that approval request.');
			});
			this.pendingApproval = null;
		} finally {
			this.isRespondingToApproval = false;
		}
	}

	/** Drops everything for a conversation being deleted or reloaded. */
	clear(messageIds?: string[]): void {
		if (!messageIds) {
			this.activities.clear();
			this.pendingApproval = null;
			return;
		}
		for (const id of messageIds) this.activities.delete(id);
	}
}

export const agentStore = new AgentStore();
export const agentActivityFor = (messageId: string): AgentActivity | undefined =>
	agentStore.activityFor(messageId);
