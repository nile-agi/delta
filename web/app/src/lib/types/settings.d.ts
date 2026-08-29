import type { SETTING_CONFIG_DEFAULT } from '$lib/constants/settings-config';
import type { ChatMessageTimings } from './chat';
import type { AgentEvent } from './agent';

export type SettingsConfigValue = string | number | boolean;

export interface SettingsFieldConfig {
	key: string;
	label: string;
	type: 'input' | 'textarea' | 'checkbox' | 'select';
	help?: string;
	options?: Array<{ value: string; label: string; icon?: typeof import('@lucide/svelte').Icon }>;
}

export interface SettingsChatServiceOptions {
	stream?: boolean;
	// Generation parameters
	temperature?: number;
	max_tokens?: number;
	// Sampling parameters
	dynatemp_range?: number;
	dynatemp_exponent?: number;
	top_k?: number;
	top_p?: number;
	min_p?: number;
	xtc_probability?: number;
	xtc_threshold?: number;
	typ_p?: number;
	// Penalty parameters
	repeat_last_n?: number;
	repeat_penalty?: number;
	presence_penalty?: number;
	frequency_penalty?: number;
	dry_multiplier?: number;
	dry_base?: number;
	dry_allowed_length?: number;
	dry_penalty_last_n?: number;
	// Sampler configuration
	samplers?: string | string[];
	// Custom parameters
	custom?: string;
	// Agent tools
	useTools?: boolean;
	/**
	 * Tool categories the harness may use this run. Omitted categories default to enabled, so a
	 * caller that sets nothing gets the full tool set.
	 */
	useCalendarTools?: boolean;
	useNotesTools?: boolean;
	useMemoryTools?: boolean;
	useTaskTools?: boolean;
	useFileTools?: boolean;
	useShellTools?: boolean;
	useWebTools?: boolean;
	/** Cap on harness iterations (model call -> tools -> model call) for one turn. */
	max_iterations?: number;
	// Timing display
	timings_per_token?: boolean;
	// Callbacks
	onChunk?: (chunk: string) => void;
	onReasoningChunk?: (chunk: string) => void;
	onModel?: (model: string) => void;
	onFirstValidChunk?: () => void;
	onComplete?: (
		response: string,
		reasoningContent?: string,
		timings?: ChatMessageTimings,
		toolCalls?: DatabaseMessageToolCall[]
	) => void;
	onError?: (error: Error) => void;
	/** Tool activity, context compaction, and approval requests from the harness. */
	onAgentEvent?: (event: AgentEvent) => void;
}

export type SettingsConfigType = typeof SETTING_CONFIG_DEFAULT & {
	[key: string]: SettingsConfigValue;
};
