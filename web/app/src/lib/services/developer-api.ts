import { config, updateConfig } from '$lib/stores/settings.svelte';
import { getModelApiBaseUrl } from '$lib/utils/model-api-url';

/** The OpenAI-compatible API's address and access, as the engine reports it. */
export interface ApiServerState {
	network: boolean;
	host: string;
	port: number;
	local_url: string;
	network_url: string | null;
	has_key: boolean;
	api_key: string;
	/** Command-line flags are in effect, so saved changes apply on the next start. */
	overridden: boolean;
}

export interface ApiServerChange {
	network?: boolean;
	key?: 'generate' | 'remove';
}

async function readJson<T>(response: Response): Promise<T> {
	const body = await response.json().catch(() => ({}));
	if (!response.ok) {
		const message = body?.error?.message ?? `Request failed (HTTP ${response.status})`;
		throw new Error(message);
	}
	return body as T;
}

export async function getApiServer(): Promise<ApiServerState> {
	return readJson(await fetch(`${getModelApiBaseUrl()}/api/developer/server`));
}

export async function updateApiServer(change: ApiServerChange): Promise<ApiServerState> {
	const state = await readJson<ApiServerState>(
		await fetch(`${getModelApiBaseUrl()}/api/developer/server`, {
			method: 'PUT',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify(change)
		})
	);
	rememberKey(state);
	return state;
}

/** Model ids an API client can pass as `model`. */
export async function listApiModels(apiKey: string): Promise<string[]> {
	const headers: Record<string, string> = apiKey ? { Authorization: `Bearer ${apiKey}` } : {};
	const body = await readJson<{ data?: Array<{ id: string }> }>(
		await fetch(`${getModelApiBaseUrl()}/v1/models`, { headers })
	);
	return (body.data ?? []).map((model) => model.id);
}

// Delta's own chat calls the same API, so it carries whatever key the API now requires.
function rememberKey(state: ApiServerState): void {
	const key = state.has_key ? state.api_key : '';
	if ((config().apiKey ?? '') !== key) updateConfig('apiKey', key);
}

/** Picks up a key set from the CLI or another window, so chats are not refused. */
export async function syncApiKey(): Promise<void> {
	try {
		rememberKey(await getApiServer());
	} catch {
		// An older engine has no Developer routes; nothing to sync.
	}
}

export interface ApiSnippets {
	curl: string;
	python: string;
	javascript: string;
}

export function apiSnippets(baseUrl: string, model: string, hasKey: boolean): ApiSnippets {
	const key = hasKey ? 'YOUR_API_KEY' : 'delta';
	const auth = hasKey ? `\n  -H "Authorization: Bearer ${key}" \\` : '';
	return {
		curl: `curl ${baseUrl}/chat/completions \\${auth}
  -H "Content-Type: application/json" \\
  -d '{"model": "${model}", "messages": [{"role": "user", "content": "Hello"}]}'`,
		python: `from openai import OpenAI

client = OpenAI(base_url="${baseUrl}", api_key="${key}")
reply = client.chat.completions.create(
    model="${model}",
    messages=[{"role": "user", "content": "Hello"}],
)
print(reply.choices[0].message.content)`,
		javascript: `import OpenAI from "openai";

const client = new OpenAI({ baseURL: "${baseUrl}", apiKey: "${key}" });
const reply = await client.chat.completions.create({
  model: "${model}",
  messages: [{ role: "user", content: "Hello" }],
});
console.log(reply.choices[0].message.content);`
	};
}
