/**
 * Events the Delta harness streams alongside the regular OpenAI chunks.
 *
 * The engine emits them as SSE frames whose `object` is `delta.agent.event`, on the same stream
 * as `chat.completion.chunk` frames. Assistant text arrives as a normal chunk, so a client that
 * ignores these frames still renders the reply -- they add the activity around it: which tools
 * ran, what the harness had to drop from context, and when it needs an answer from the user.
 */

export type AgentToolRisk = 'safe' | 'caution' | 'destructive';

export type AgentEventName =
	| 'tool_start'
	| 'tool_result'
	| 'approval_required'
	| 'approval_resolved'
	| 'compaction'
	| 'status'
	| 'error'
	| 'run_summary';

export interface AgentToolStartData {
	name: string;
	arguments: Record<string, unknown>;
	risk: AgentToolRisk;
}

export interface AgentToolResultData {
	name: string;
	success: boolean;
	summary?: string;
	error?: string;
}

export interface AgentApprovalRequiredData {
	id: string;
	name: string;
	arguments: Record<string, unknown>;
	risk: AgentToolRisk;
	description?: string;
}

export interface AgentApprovalResolvedData {
	id: string;
	/** `timeout` when the user never answered. */
	decision: 'allow' | 'always' | 'deny' | 'never' | 'timeout';
}

export interface AgentCompactionData {
	dropped: number;
	summarized: boolean;
	truncated_results: number;
	used_tokens: number;
	budget_tokens: number;
}

export interface AgentStatusData {
	message: string;
}

export interface AgentRunSummaryData {
	tool_calls: number;
	iterations: number;
	stop_reason: string;
	tools: Array<{ name: string; arguments: Record<string, unknown>; success: boolean }>;
}

export type AgentEventData =
	| AgentToolStartData
	| AgentToolResultData
	| AgentApprovalRequiredData
	| AgentApprovalResolvedData
	| AgentCompactionData
	| AgentStatusData
	| AgentRunSummaryData;

export interface AgentEvent {
	object: 'delta.agent.event';
	event: AgentEventName;
	data: AgentEventData;
}

/** The decisions the approval prompt offers. */
export type AgentApprovalDecision = 'allow' | 'always' | 'deny' | 'never';

/**
 * One tool invocation as the UI shows it. Built from a `tool_start` and then completed by the
 * matching `tool_result`, so a running tool is visible while it works.
 */
export interface AgentActivityStep {
	id: string;
	name: string;
	arguments: Record<string, unknown>;
	risk: AgentToolRisk;
	status: 'running' | 'awaiting-approval' | 'success' | 'failed' | 'denied';
	summary?: string;
	error?: string;
	/** Set while this step is waiting on the user; the id the approval is answered with. */
	approvalId?: string;
	startedAt: number;
	endedAt?: number;
}

/** Everything the harness reported during one assistant turn. */
export interface AgentActivity {
	steps: AgentActivityStep[];
	notices: string[];
	compaction?: AgentCompactionData;
	stopReason?: string;
	iterations?: number;
}
