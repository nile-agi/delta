import type { ApiModelDataEntry, ApiModelDetails } from '$lib/types/api';

export interface ModelOption {
	id: string;
	name: string;
	model: string;
	/** File stem llama-server reports in router mode; saved replies may name the model by it. */
	alias?: string;
	description?: string;
	capabilities: string[];
	details?: ApiModelDetails['details'];
	meta?: ApiModelDataEntry['meta'];
}
