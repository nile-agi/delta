/**
 * Model Catalog - Hardware-aware model catalog for Delta
 *
 * This catalog organizes models into families with descriptions, download URLs,
 * hardware requirements, and metadata. Models are grouped by family for easy
 * browsing and hardware-aware filtering.
 */

export interface ModelCatalogModel {
	name: string;
	display_name: string;
	download_url: string; // HuggingFace GGUF repository path
	file_size_gb: number;
	context_size: number; // in tokens (e.g., 8192, 32768, 131072)
	required_ram_gb: number; // Estimated: file_size_gb * 1.5 + 2GB overhead
	quantization?: string; // e.g., "Q4_K_M", "Q8_0", "F16"
	icon?: string; // Emoji or icon identifier
}

export interface ModelFamily {
	id: string;
	name: string;
	description: string;
	icon: string; // Emoji or icon identifier
	models: ModelCatalogModel[];
}

/**
 * Calculate required RAM based on file size
 * Formula: file_size_gb * 1.5 + 2GB overhead
 */
function calculateRequiredRAM(fileSizeGB: number): number {
	return Math.ceil(fileSizeGB * 1.5 + 2);
}

/**
 * Model Catalog - Organized by families
 */
export const modelsCatalog: ModelFamily[] = [
	{
		id: 'ministral-3',
		name: 'Ministral 3',
		description:
			"Mistral AI's compact edge models with vision capabilities. Offers best cost-to-performance ratio for on-device deployment in 3B, 8B, 14B sizes.",
		icon: 'M',
		models: [
			{
				name: 'ministral-3:3b',
				display_name: 'Ministral 3 3B',
				download_url: 'mistralai/Ministral-3-3B-Instruct-GGUF',
				file_size_gb: 2.1,
				context_size: 16384,
				required_ram_gb: calculateRequiredRAM(2.1),
				quantization: 'Q4_K_M'
			},
			{
				name: 'ministral-3:8b',
				display_name: 'Ministral 3 8B',
				download_url: 'mistralai/Ministral-3-8B-Instruct-GGUF',
				file_size_gb: 5.5,
				context_size: 16384,
				required_ram_gb: calculateRequiredRAM(5.5),
				quantization: 'Q4_K_M'
			},
			{
				name: 'ministral-3:14b',
				display_name: 'Ministral 3 14B',
				download_url: 'mistralai/Ministral-3-14B-Instruct-GGUF',
				file_size_gb: 9.5,
				context_size: 16384,
				required_ram_gb: calculateRequiredRAM(9.5),
				quantization: 'Q4_K_M'
			}
		]
	},
	{
		id: 'qwen3-vl',
		name: 'Qwen3 VL',
		description:
			"Alibaba's vision-language model for understanding text, images, and video. Features OCR in 32 languages, GUI agents, and spatial reasoning.",
		icon: '∇',
		models: [
			{
				name: 'qwen3-vl:2b',
				display_name: 'Qwen3 VL 2B',
				download_url: 'Qwen/Qwen3-VL-2B-Instruct-GGUF',
				file_size_gb: 1.8,
				context_size: 8192,
				required_ram_gb: calculateRequiredRAM(1.8),
				quantization: 'Q4_K_M'
			},
			{
				name: 'qwen3-vl:4b',
				display_name: 'Qwen3 VL 4B',
				download_url: 'Qwen/Qwen3-VL-4B-Instruct-GGUF',
				file_size_gb: 2.5,
				context_size: 8192,
				required_ram_gb: calculateRequiredRAM(2.5),
				quantization: 'Q4_K_M'
			},
			{
				name: 'qwen3-vl:8b',
				display_name: 'Qwen3 VL 8B',
				download_url: 'Qwen/Qwen3-VL-8B-Instruct-GGUF',
				file_size_gb: 5.5,
				context_size: 8192,
				required_ram_gb: calculateRequiredRAM(5.5),
				quantization: 'Q4_K_M'
			},
			{
				name: 'qwen3-vl:30b-a3b',
				display_name: 'Qwen3 VL 30B-A3B',
				download_url: 'Qwen/Qwen3-VL-30B-A3B-Instruct-GGUF',
				file_size_gb: 20.0,
				context_size: 8192,
				required_ram_gb: calculateRequiredRAM(20.0),
				quantization: 'Q4_K_M'
			}
		]
	},
	{
		id: 'gemma-3',
		name: 'Gemma 3',
		description:
			"Google's multimodal models built from Gemini technology. Supports 140+ languages, vision, and text tasks with up to 128K context for edge to cloud deployment.",
		icon: '◇',
		models: [
			{
				name: 'gemma3:270m',
				display_name: 'Gemma 3 270M',
				download_url: 'google/gemma-3-270m-it-GGUF',
				file_size_gb: 0.2,
				context_size: 32768,
				required_ram_gb: calculateRequiredRAM(0.2),
				quantization: 'Q8_0'
			},
			{
				name: 'gemma3:1b',
				display_name: 'Gemma 3 1B',
				download_url: 'google/gemma-3-1b-it-GGUF',
				file_size_gb: 0.7,
				context_size: 8192,
				required_ram_gb: calculateRequiredRAM(0.7),
				quantization: 'Q8_0'
			},
			{
				name: 'gemma3:4b',
				display_name: 'Gemma 3 4B',
				download_url: 'google/gemma-3-4b-it-GGUF',
				file_size_gb: 2.5,
				context_size: 32768,
				required_ram_gb: calculateRequiredRAM(2.5),
				quantization: 'Q4_K_M'
			},
			{
				name: 'gemma3:12b',
				display_name: 'Gemma 3 12B',
				download_url: 'google/gemma-3-12b-it-GGUF',
				file_size_gb: 7.3,
				context_size: 32768,
				required_ram_gb: calculateRequiredRAM(7.3),
				quantization: 'Q4_K_M'
			},
			{
				name: 'gemma3:27b',
				display_name: 'Gemma 3 27B',
				download_url: 'google/gemma-3-27b-it-GGUF',
				file_size_gb: 16.5,
				context_size: 32768,
				required_ram_gb: calculateRequiredRAM(16.5),
				quantization: 'Q4_K_M'
			}
		]
	},
	{
		id: 'qwen3',
		name: 'Qwen3',
		description:
			"Alibaba's hybrid reasoning models with thinking/non-thinking modes. Excels at coding, math, and multilingual tasks across 119 languages.",
		icon: '∇',
		models: [
			{
				name: 'qwen3:0.6b',
				display_name: 'Qwen3 0.6B',
				download_url: 'Qwen/Qwen3-0.6B-GGUF',
				file_size_gb: 1.5,
				context_size: 32768,
				required_ram_gb: calculateRequiredRAM(1.5),
				quantization: 'F16'
			},
			{
				name: 'qwen3:1.7b',
				display_name: 'Qwen3 1.7B',
				download_url: 'Qwen/Qwen3-1.7B-GGUF',
				file_size_gb: 1.1,
				context_size: 32768,
				required_ram_gb: calculateRequiredRAM(1.1),
				quantization: 'F16'
			},
			{
				name: 'qwen3:4b',
				display_name: 'Qwen3 4B',
				download_url: 'Qwen/Qwen3-4B-GGUF',
				file_size_gb: 2.5,
				context_size: 32768,
				required_ram_gb: calculateRequiredRAM(2.5),
				quantization: 'Q4_K_M'
			},
			{
				name: 'qwen3:8b',
				display_name: 'Qwen3 8B',
				download_url: 'Qwen/Qwen3-8B-GGUF',
				file_size_gb: 5.0,
				context_size: 32768,
				required_ram_gb: calculateRequiredRAM(5.0),
				quantization: 'Q4_K_M'
			},
			{
				name: 'qwen3:14b',
				display_name: 'Qwen3 14B',
				download_url: 'Qwen/Qwen3-14B-GGUF',
				file_size_gb: 9.0,
				context_size: 32768,
				required_ram_gb: calculateRequiredRAM(9.0),
				quantization: 'Q4_K_M'
			},
			{
				name: 'qwen3:30b-a3b',
				display_name: 'Qwen3 30B-A3B',
				download_url: 'Qwen/Qwen3-30B-A3B-GGUF',
				file_size_gb: 20.0,
				context_size: 32768,
				required_ram_gb: calculateRequiredRAM(20.0),
				quantization: 'Q4_K_M'
			}
		]
	},
	{
		id: 'glm-4.7',
		name: 'GLM 4.7',
		description:
			"Zhipu AI's agentic reasoning and coding models. Built for software engineering, browser automation, and multi-turn tool use.",
		icon: 'Z',
		models: [
			{
				name: 'glm-4.7:flash',
				display_name: 'GLM 4.7 Flash',
				download_url: 'THUDM/glm-4-7-flash-GGUF',
				file_size_gb: 2.0,
				context_size: 131072,
				required_ram_gb: calculateRequiredRAM(2.0),
				quantization: 'Q4_K_M'
			}
		]
	},
	{
		id: 'devstral-2',
		name: 'Devstral 2',
		description:
			"Mistral AI's agentic coding models for software engineering tasks. Excels at exploring codebases, multi-file editing, and powering code agents.",
		icon: 'D',
		models: [
			{
				name: 'devstral-2:24b',
				display_name: 'Devstral 2 24B',
				download_url: 'mistralai/Devstral-2-24B-Instruct-GGUF',
				file_size_gb: 15.0,
				context_size: 131072,
				required_ram_gb: calculateRequiredRAM(15.0),
				quantization: 'Q4_K_M'
			},
			{
				name: 'devstral-2:123b',
				display_name: 'Devstral 2 123B',
				download_url: 'mistralai/Devstral-2-123B-Instruct-GGUF',
				file_size_gb: 75.0,
				context_size: 131072,
				required_ram_gb: calculateRequiredRAM(75.0),
				quantization: 'Q4_K_M'
			}
		]
	},
	{
		id: 'nemotron-nano-3',
		name: 'Nemotron Nano 3',
		description:
			"NVIDIA's efficient hybrid MoE model for agentic AI. Built for reasoning, coding, and tool use with 1M token context and 4x faster throughput.",
		icon: 'N',
		models: [
			{
				name: 'nemotron-nano-3:30b-a3b',
				display_name: 'Nemotron Nano 3 30B-A3B',
				download_url: 'nvidia/Nemotron-Nano-3-30B-A3B-GGUF',
				file_size_gb: 20.0,
				context_size: 1048576,
				required_ram_gb: calculateRequiredRAM(20.0),
				quantization: 'Q4_K_M'
			}
		]
	},
	{
		id: 'gpt-oss',
		name: 'GPT-OSS',
		description:
			"OpenAI's first open-weight models since GPT-2. Built for reasoning, agentic tasks, and developer use with function calling and tool use capabilities.",
		icon: 'G',
		models: [
			{
				name: 'gpt-oss:20b',
				display_name: 'GPT-OSS 20B',
				download_url: 'openai/gpt-oss-20b-GGUF',
				file_size_gb: 12.0,
				context_size: 131072,
				required_ram_gb: calculateRequiredRAM(12.0),
				quantization: 'Q4_K_M'
			},
			{
				name: 'gpt-oss:120b',
				display_name: 'GPT-OSS 120B',
				download_url: 'openai/gpt-oss-120b-GGUF',
				file_size_gb: 70.0,
				context_size: 131072,
				required_ram_gb: calculateRequiredRAM(70.0),
				quantization: 'Q4_K_M'
			}
		]
	},
	{
		id: 'qwen3-coder',
		name: 'Qwen3 Coder',
		description:
			"Alibaba's specialized coding model for agentic software engineering. Optimized for function calling, tool use, and repository-scale reasoning.",
		icon: '∇',
		models: [
			{
				name: 'qwen3-coder:30b-a3b',
				display_name: 'Qwen3 Coder 30B-A3B',
				download_url: 'Qwen/Qwen3-Coder-30B-A3B-GGUF',
				file_size_gb: 20.0,
				context_size: 131072,
				required_ram_gb: calculateRequiredRAM(20.0),
				quantization: 'Q4_K_M'
			}
		]
	}
];

/**
 * Get all models from all families
 */
export function getAllModels(): ModelCatalogModel[] {
	return modelsCatalog.flatMap((family) => family.models);
}

/**
 * Find a model by name
 */
export function findModelByName(name: string): ModelCatalogModel | undefined {
	return getAllModels().find((model) => model.name === name);
}

/**
 * Get family icon for a model by name (shared between Catalog and Installed).
 * LlamaBarn-style: ◇ Gemma, ∇ Qwen, M Ministral/Mistral, etc.
 */
export function getFamilyIconForModelName(name: string): string {
	const catalogModel = findModelByName(name);
	if (catalogModel) {
		for (const family of modelsCatalog) {
			if (family.models.some((m) => m.name === name)) return family.icon;
		}
	}
	const lower = name.toLowerCase();
	if (lower.includes('gemma')) return '◇';
	if (lower.includes('qwen')) return '∇';
	if (lower.includes('ministral') || lower.includes('mistral')) return 'M';
	if (lower.includes('glm')) return 'Z';
	if (lower.includes('devstral')) return 'D';
	if (lower.includes('nemotron')) return 'N';
	if (lower.includes('gpt')) return 'G';
	return '●';
}

/**
 * Parse params in billions from model name (e.g. "270M" → 0.27, "4B" → 4).
 * Used for optional KV-cache-style mem estimates; returns null if unparseable.
 */
export function parseParamsBillionsFromModelName(name: string): number | null {
	const m = name.match(/(\d+(?:\.\d+)?)\s*([MmBb])/i);
	if (!m) return null;
	const num = parseFloat(m[1]);
	const unit = m[2].toUpperCase();
	if (unit === 'M') return num / 1000;
	if (unit === 'B') return num;
	return null;
}

/**
 * Get smallest compatible model based on available RAM
 */
export function getSmallestCompatibleModel(availableRAMGB: number): ModelCatalogModel | null {
	const compatibleModels = getAllModels().filter(
		(model) => model.required_ram_gb <= availableRAMGB
	);

	if (compatibleModels.length === 0) {
		return null;
	}

	// Sort by file size (smallest first)
	compatibleModels.sort((a, b) => a.file_size_gb - b.file_size_gb);
	return compatibleModels[0];
}

/**
 * Standard context length options (tokens) for UI radio buttons.
 * LlamaBarn-style: 4k, 8k, 16k, 32k (cap by model max if provided).
 */
export const CONTEXT_OPTIONS = [4096, 8192, 16384, 32768] as const;

export function getContextOptionsForModel(maxContextTokens?: number): number[] {
	if (maxContextTokens == null || maxContextTokens <= 0) {
		return [...CONTEXT_OPTIONS];
	}
	return CONTEXT_OPTIONS.filter((ctx) => ctx <= maxContextTokens);
}

/**
 * Estimate memory (GB) for a model at a given context length.
 * LlamaBarn-style: "Xk ctx on Y.Y GB mem" with small increases for larger ctx.
 *
 * Formula (matches LlamaBarn screenshots):
 * - base_mem: small models (<1 GB file) use 1.5× file size (e.g. 0.2 → 0.3 GB);
 *   larger models use 1.24× (model + quant overhead, e.g. 2.5 → 3.1 GB).
 * - KV cache: grows with context; log2 scaling so doubling ctx adds ~fixed extra.
 *   kvExtra = log2(ctx/4096) * k * fileSizeGB with k=0.165 for small, 0.08 for large.
 *   Result: small +0.0–0.1 GB per doubling; larger +0.2–0.6 GB (e.g. Gemma 4B 3.1→3.7).
 * Filtering: getContextOptionsForModel caps by model max_ctx; UI grays out options
 * where estimateMemoryGB(...) > system RAM and shows tooltip "Requires X.X GB+".
 * Rounded to 1 decimal for display.
 */
export function estimateMemoryGB(fileSizeGB: number, contextTokens: number): number {
	const isSmall = fileSizeGB < 1;
	const base = fileSizeGB * (isSmall ? 1.5 : 1.24);
	if (contextTokens <= 4096) return Math.round(base * 10) / 10;
	const log2Factor = Math.log2(contextTokens / 4096);
	const kvCoeff = isSmall ? 0.165 : 0.08;
	const kvExtra = log2Factor * kvCoeff * fileSizeGB;
	return Math.round((base + kvExtra) * 10) / 10;
}

/**
 * Get quantization suggestions for a model if base exceeds RAM
 */
export function getQuantizationSuggestions(
	model: ModelCatalogModel,
	availableRAMGB: number
): ModelCatalogModel[] {
	if (model.required_ram_gb <= availableRAMGB) {
		return [];
	}

	// Suggest Q4 and Q5 variants if base model is too large
	const suggestions: ModelCatalogModel[] = [];

	// Q4 variant (smaller)
	const q4Model: ModelCatalogModel = {
		...model,
		name: `${model.name}-q4`,
		display_name: `${model.display_name} (Q4)`,
		file_size_gb: model.file_size_gb * 0.6, // Q4 is ~60% of original
		required_ram_gb: calculateRequiredRAM(model.file_size_gb * 0.6),
		quantization: 'Q4_K_M'
	};

	if (q4Model.required_ram_gb <= availableRAMGB) {
		suggestions.push(q4Model);
	}

	// Q5 variant (medium)
	const q5Model: ModelCatalogModel = {
		...model,
		name: `${model.name}-q5`,
		display_name: `${model.display_name} (Q5)`,
		file_size_gb: model.file_size_gb * 0.7, // Q5 is ~70% of original
		required_ram_gb: calculateRequiredRAM(model.file_size_gb * 0.7),
		quantization: 'Q5_K_M'
	};

	if (q5Model.required_ram_gb <= availableRAMGB) {
		suggestions.push(q5Model);
	}

	return suggestions;
}
