import { config } from '$lib/stores/settings.svelte';
import { selectedModelName, selectedModelOption } from '$lib/stores/models.svelte';
import { getModelApiBaseUrl } from '$lib/utils/model-api-url';
import { getServerBaseUrl } from '$lib/utils/server-base-url';
import { slotsService } from './slots';

const IS_TAURI =
	typeof window !== 'undefined' &&
	('__TAURI_INTERNALS__' in window || '__TAURI__' in window);

function flattenContent(content: unknown): string {
	if (typeof content === 'string') return content;
	if (Array.isArray(content)) {
		return content
			.filter((p: Record<string, unknown>) => p.type === 'text' && p.text)
			.map((p: Record<string, unknown>) => p.text as string)
			.join('\n');
	}
	if (content && typeof content === 'object') {
		const obj = content as Record<string, unknown>;
		if (obj.type === 'text' && typeof obj.text === 'string') return obj.text;
		return JSON.stringify(content);
	}
	return content == null ? '' : String(content);
}
/**
 * ChatService - Low-level API communication layer for Delta server interactions
 *
 * This service handles direct communication with the Delta server's chat completion API.
 * It provides the network layer abstraction for AI model interactions while remaining
 * stateless and focused purely on API communication.
 *
 * **Architecture & Relationship with ChatStore:**
 * - **ChatService** (this class): Stateless API communication layer
 *   - Handles HTTP requests/responses with Delta server
 *   - Manages streaming and non-streaming response parsing
 *   - Provides request abortion capabilities
 *   - Converts database messages to API format
 *   - Handles error translation for server responses
 *
 * - **ChatStore**: Stateful orchestration and UI state management
 *   - Uses ChatService for all AI model communication
 *   - Manages conversation state, message history, and UI reactivity
 *   - Coordinates with DatabaseStore for persistence
 *   - Handles complex workflows like branching and regeneration
 *
 * **Key Responsibilities:**
 * - Message format conversion (DatabaseMessage → API format)
 * - Streaming response handling with real-time callbacks
 * - Reasoning content extraction and processing
 * - File attachment processing (images, PDFs, audio, text)
 * - Request lifecycle management (abort, cleanup)
 */
export class ChatService {
	private abortControllers: Map<string, AbortController> = new Map();

	/**
	 * Sends a chat completion request to the Delta server.
	 * Supports both streaming and non-streaming responses with comprehensive parameter configuration.
	 * Automatically converts database messages with attachments to the appropriate API format.
	 *
	 * @param messages - Array of chat messages to send to the API (supports both ApiChatMessageData and DatabaseMessage with attachments)
	 * @param options - Configuration options for the chat completion request. See `SettingsChatServiceOptions` type for details.
	 * @returns {Promise<string | void>} that resolves to the complete response string (non-streaming) or void (streaming)
	 * @throws {Error} if the request fails or is aborted
	 */
	async sendMessage(
		messages: ApiChatMessageData[] | (DatabaseMessage & { extra?: DatabaseMessageExtra[] })[],
		options: SettingsChatServiceOptions = {},
		conversationId?: string
	): Promise<string | void> {
		const {
			stream,
			onChunk,
			onComplete,
			onError,
			onReasoningChunk,
			onModel,
			onFirstValidChunk,
			useTools,
			useCalendarTools,
			useNotesTools,
			useMemoryTools,
			useTaskTools,
			useFileTools,
			useShellTools,
			useWebTools,
			max_iterations,
			onAgentEvent,
			// Generation parameters
			temperature,
			max_tokens,
			// Sampling parameters
			dynatemp_range,
			dynatemp_exponent,
			top_k,
			top_p,
			min_p,
			xtc_probability,
			xtc_threshold,
			typ_p,
			// Penalty parameters
			repeat_last_n,
			repeat_penalty,
			presence_penalty,
			frequency_penalty,
			dry_multiplier,
			dry_base,
			dry_allowed_length,
			dry_penalty_last_n,
			// Other parameters
			samplers,
			custom,
			timings_per_token
		} = options;

		const currentConfig = config();

		const requestId = conversationId || 'default';

		if (this.abortControllers.has(requestId)) {
			this.abortControllers.get(requestId)?.abort();
		}

		const abortController = new AbortController();
		this.abortControllers.set(requestId, abortController);

		const normalizedMessages: ApiChatMessageData[] = messages
			.flatMap((msg): ApiChatMessageData[] => {
				if ('id' in msg && 'convId' in msg && 'timestamp' in msg) {
					const dbMsg = msg as DatabaseMessage & { extra?: DatabaseMessageExtra[] };
					// An assistant turn the harness produced with tools is replayed as the full
					// record (tool calls and results) so the model can still see what they returned.
					const transcript = dbMsg.agent_activity?.transcript;
					if (useTools && dbMsg.role === 'assistant' && transcript?.length) {
						return transcript.map((m: ApiChatMessageData) => ({ ...m }));
					}
					return [ChatService.convertMessageToChatServiceData(dbMsg)];
				} else {
					return [msg as ApiChatMessageData];
				}
			})
			.filter((msg) => {
				if (msg.role === 'system') {
					const content = typeof msg.content === 'string' ? msg.content : '';

					return content.trim().length > 0;
				}

				return true;
			});

		const processedMessages = this.injectSystemMessage(normalizedMessages);
		const alternatingMessages = ChatService.ensureAlternatingRoles(processedMessages);

		const requestBody: ApiChatCompletionRequest = {
			messages: alternatingMessages.map((msg: ApiChatMessageData) => ({
				role: msg.role,
				content: useTools ? flattenContent(msg.content) : msg.content,
				...(msg.tool_calls ? { tool_calls: msg.tool_calls } : {}),
				...(msg.tool_call_id ? { tool_call_id: msg.tool_call_id } : {}),
				...(msg.name ? { name: msg.name } : {})
			})),
			stream
		};

		if (useTools) {
			// Each category defaults to on, so leaving these unset gives the model everything.
			requestBody.use_tools = true;
			if (conversationId) requestBody.conversation_id = conversationId;
			requestBody.use_calendar_tools = useCalendarTools !== false;
			requestBody.use_notes_tools = useNotesTools !== false;
			requestBody.use_memory_tools = useMemoryTools !== false;
			requestBody.use_task_tools = useTaskTools !== false;
			requestBody.use_files_tools = useFileTools !== false;
			requestBody.use_shell_tools = useShellTools !== false;
			requestBody.use_web_tools = useWebTools !== false;
			if (max_iterations !== undefined) requestBody.max_iterations = max_iterations;
		}

		const selectedOption = selectedModelOption();
		const activeModel = selectedModelName() ?? selectedOption?.model ?? null;
		if (activeModel) {
			requestBody.model = activeModel;
		}

		requestBody.reasoning_format = currentConfig.disableReasoningFormat ? 'none' : 'auto';
		if (currentConfig.backendSampling) requestBody.backend_sampling = true;

		if (temperature !== undefined) requestBody.temperature = temperature;
		if (max_tokens !== undefined) {
			// Set max_tokens to -1 (infinite) when explicitly configured as 0 or null
			requestBody.max_tokens = max_tokens !== null && max_tokens !== 0 ? max_tokens : -1;
		}

		if (dynatemp_range !== undefined) requestBody.dynatemp_range = dynatemp_range;
		if (dynatemp_exponent !== undefined) requestBody.dynatemp_exponent = dynatemp_exponent;
		if (top_k !== undefined) requestBody.top_k = top_k;
		if (top_p !== undefined) requestBody.top_p = top_p;
		if (min_p !== undefined) requestBody.min_p = min_p;
		if (xtc_probability !== undefined) requestBody.xtc_probability = xtc_probability;
		if (xtc_threshold !== undefined) requestBody.xtc_threshold = xtc_threshold;
		if (typ_p !== undefined) requestBody.typ_p = typ_p;

		if (repeat_last_n !== undefined) requestBody.repeat_last_n = repeat_last_n;
		if (repeat_penalty !== undefined) requestBody.repeat_penalty = repeat_penalty;
		if (presence_penalty !== undefined) requestBody.presence_penalty = presence_penalty;
		if (frequency_penalty !== undefined) requestBody.frequency_penalty = frequency_penalty;
		if (dry_multiplier !== undefined) requestBody.dry_multiplier = dry_multiplier;
		if (dry_base !== undefined) requestBody.dry_base = dry_base;
		if (dry_allowed_length !== undefined) requestBody.dry_allowed_length = dry_allowed_length;
		if (dry_penalty_last_n !== undefined) requestBody.dry_penalty_last_n = dry_penalty_last_n;

		if (samplers !== undefined) {
			requestBody.samplers =
				typeof samplers === 'string'
					? samplers.split(';').filter((s: string) => s.trim())
					: samplers;
		}

		if (timings_per_token !== undefined) requestBody.timings_per_token = timings_per_token;

		if (custom) {
			try {
				const customParams = typeof custom === 'string' ? JSON.parse(custom) : custom;
				Object.assign(requestBody, customParams);
			} catch (error) {
				console.warn('Failed to parse custom parameters:', error);
			}
		}

		try {
			const apiKey = currentConfig.apiKey?.toString().trim();
			// Tools live in the agent loop, which only the model API server hosts. getModelApiBaseUrl()
			// resolves it in every mode (Tauri, same-origin UI-only, and port+1 after the 8080 migration).
			const baseUrl = useTools ? getModelApiBaseUrl() : getServerBaseUrl();
			const url = `${baseUrl}/v1/chat/completions`;
			const headers: Record<string, string> = {
				'Content-Type': 'application/json',
				...(apiKey ? { Authorization: `Bearer ${apiKey}` } : {})
			};

			if (stream) {
				const body = JSON.stringify(requestBody);

				if (IS_TAURI) {
					try {
						await this.handleStreamResponseTauri(
							url, headers, body, onAgentEvent, onChunk, onComplete, onError,
							onReasoningChunk, onModel, onFirstValidChunk,
							conversationId, abortController.signal
						);
						return;
					} catch (tauriErr) {
						if (tauriErr instanceof Error && tauriErr.name === 'AbortError') throw tauriErr;
						console.warn('[Delta] Tauri IPC streaming failed, falling back to XHR:', tauriErr);
					}
				}
				await this.handleStreamResponseXHR(
					url, headers, body, onAgentEvent, onChunk, onComplete, onError,
					onReasoningChunk, onModel, onFirstValidChunk,
					conversationId, abortController.signal
				);
				return;
			} else {
				const response = await fetch(url, {
					method: 'POST',
					headers,
					body: JSON.stringify(requestBody),
					signal: abortController.signal
				});

				if (!response.ok) {
					throw await this.parseErrorResponse(response);
				}

				return this.handleNonStreamResponse(response, onComplete, onModel);
			}
		} catch (error) {
			if (error instanceof Error && error.name === 'AbortError') {
				console.log('Chat completion request was aborted');
				if (onError) onError(error);
				return;
			}

			let userFriendlyError: Error;

			if (error instanceof Error) {
				if (error.name === 'TypeError' && error.message.includes('fetch')) {
					userFriendlyError = new Error(
						'Unable to connect to server - please check if the server is running'
					);
					userFriendlyError.name = 'NetworkError';
				} else if (error.message.includes('ECONNREFUSED')) {
					userFriendlyError = new Error('Connection refused - server may be offline');
					userFriendlyError.name = 'NetworkError';
				} else if (error.message.includes('ETIMEDOUT')) {
					userFriendlyError = new Error('Request timed out - the server took too long to respond');
					userFriendlyError.name = 'TimeoutError';
				} else {
					userFriendlyError = error;
				}
			} else {
				userFriendlyError = new Error('Unknown error occurred while sending message');
			}

			console.error('Error in sendMessage:', error);
			if (onError) {
				onError(userFriendlyError);
			}
			throw userFriendlyError;
		} finally {
			this.abortControllers.delete(requestId);
		}
	}

	private handleStreamResponseXHR(
		url: string,
		headers: Record<string, string>,
		body: string,
		onAgentEvent: ((event: AgentEvent) => void) | undefined,
		onChunk?: (chunk: string) => void,
		onComplete?: (
			response: string,
			reasoningContent?: string,
			timings?: ChatMessageTimings,
			toolCalls?: DatabaseMessageToolCall[]
		) => void,
		onError?: (error: Error) => void,
		onReasoningChunk?: (chunk: string) => void,
		onModel?: (model: string) => void,
		onFirstValidChunk?: () => void,
		conversationId?: string,
		abortSignal?: AbortSignal
	): Promise<void> {
		return new Promise<void>((resolve, reject) => {
			if (abortSignal?.aborted) {
				resolve();
				return;
			}

			const xhr = new XMLHttpRequest();
			xhr.open('POST', url, true);
			for (const [key, value] of Object.entries(headers)) {
				xhr.setRequestHeader(key, value);
			}

			let aggregatedContent = '';
			let fullReasoningContent = '';
			const collectedToolCalls: DatabaseMessageToolCall[] = [];
			let hasReceivedData = false;
			let lastTimings: ChatMessageTimings | undefined;
			let streamFinished = false;
			let modelEmitted = false;
			let firstValidChunkEmitted = false;
			let lastProcessedIndex = 0;
			let partialLine = '';

			const processNewData = (responseText: string) => {
				const newData = responseText.substring(lastProcessedIndex);
				lastProcessedIndex = responseText.length;
				if (!newData) return;

				const text = partialLine + newData;
				const lines = text.split('\n');
				partialLine = lines.pop() || '';

				for (const line of lines) {
					if (abortSignal?.aborted) break;

					if (line.startsWith('data: ')) {
						const data = line.slice(6);
						if (data === '[DONE]') {
							streamFinished = true;
							continue;
						}

						try {
							const raw: unknown = JSON.parse(data);
							if (this.consumeAgentEvent(raw, onAgentEvent)) continue;
							const parsed = raw as ApiChatCompletionStreamChunk;

							if (!firstValidChunkEmitted && parsed.object === 'chat.completion.chunk') {
								firstValidChunkEmitted = true;
								if (!abortSignal?.aborted) {
									onFirstValidChunk?.();
								}
							}

							const content = parsed.choices[0]?.delta?.content;
							const reasoningContent = parsed.choices[0]?.delta?.reasoning_content;
							const deltaToolCalls = parsed.choices[0]?.delta?.tool_calls;
							if (deltaToolCalls?.length) collectedToolCalls.push(...deltaToolCalls);
							const timings = parsed.timings;
							const promptProgress = parsed.prompt_progress;

							const chunkModel = this.extractModelName(parsed);
							if (chunkModel && !modelEmitted) {
								modelEmitted = true;
								onModel?.(chunkModel);
							}

							if (timings || promptProgress) {
								this.updateProcessingState(timings, promptProgress, conversationId);
								if (timings) {
									lastTimings = timings;
								}
							}

							if (content) {
								hasReceivedData = true;
								aggregatedContent += content;
								if (!abortSignal?.aborted) {
									onChunk?.(content);
								}
							}

							if (reasoningContent) {
								hasReceivedData = true;
								fullReasoningContent += reasoningContent;
								if (!abortSignal?.aborted) {
									onReasoningChunk?.(reasoningContent);
								}
							}
						} catch (e) {
							console.error('Error parsing JSON chunk:', e);
						}
					}
				}
			};

			const onAbort = () => xhr.abort();
			abortSignal?.addEventListener('abort', onAbort);

			const cleanup = () => {
				abortSignal?.removeEventListener('abort', onAbort);
			};

			xhr.onprogress = () => {
				if (abortSignal?.aborted) return;
				processNewData(xhr.responseText);
			};

			xhr.onload = () => {
				processNewData(xhr.responseText);
				cleanup();

				if (xhr.status < 200 || xhr.status >= 300) {
					let error: Error;
					try {
						const errorData = JSON.parse(xhr.responseText);
						const message = errorData.error?.message || 'Unknown server error';
						error = new Error(message);
						error.name = xhr.status === 400 ? 'ServerError' : 'HttpError';
					} catch {
						error = new Error(`Server error (${xhr.status}): ${xhr.statusText}`);
						error.name = 'HttpError';
					}
					reject(error);
					return;
				}

				if (!hasReceivedData && aggregatedContent.length === 0) {
					const error = new Error('No response received from server. Please try again.');
					reject(error);
					return;
				}

				onComplete?.(
				aggregatedContent,
				fullReasoningContent || undefined,
				lastTimings,
				collectedToolCalls.length > 0 ? collectedToolCalls : undefined
			);
				resolve();
			};

			xhr.onerror = () => {
				cleanup();
				const error = new Error(
					'Unable to connect to server - please check if the server is running'
				);
				error.name = 'NetworkError';
				reject(error);
			};

			xhr.ontimeout = () => {
				cleanup();
				const error = new Error('Request timed out - the server took too long to respond');
				error.name = 'TimeoutError';
				reject(error);
			};

			xhr.onabort = () => {
				cleanup();
				const error = new Error('Request aborted');
				error.name = 'AbortError';
				reject(error);
			};

			xhr.send(body);
		});
	}

	private async handleStreamResponseTauri(
		url: string,
		headers: Record<string, string>,
		body: string,
		onAgentEvent: ((event: AgentEvent) => void) | undefined,
		onChunk?: (chunk: string) => void,
		onComplete?: (
			response: string,
			reasoningContent?: string,
			timings?: ChatMessageTimings,
			toolCalls?: DatabaseMessageToolCall[]
		) => void,
		onError?: (error: Error) => void,
		onReasoningChunk?: (chunk: string) => void,
		onModel?: (model: string) => void,
		onFirstValidChunk?: () => void,
		conversationId?: string,
		abortSignal?: AbortSignal
	): Promise<void> {
		if (abortSignal?.aborted) return;

		const { invoke, Channel } = await import('@tauri-apps/api/core');

		const absoluteUrl = url.startsWith('/')
			? `${getServerBaseUrl()}${url}`
			: url;
		const streamId = conversationId || crypto.randomUUID();
		let aggregatedContent = '';
		let fullReasoningContent = '';
		const collectedToolCalls: DatabaseMessageToolCall[] = [];
		let hasReceivedData = false;
		let lastTimings: ChatMessageTimings | undefined;
		let modelEmitted = false;
		let firstValidChunkEmitted = false;

		const onAbort = () => {
			invoke('abort_stream', { streamId }).catch(() => {});
		};
		abortSignal?.addEventListener('abort', onAbort);

		const channel = new Channel<string>();
		channel.onmessage = (line: string) => {
			if (abortSignal?.aborted) return;

			if (!line.startsWith('data: ')) return;
			const data = line.slice(6);
			if (data === '[DONE]') return;

			try {
				const raw: unknown = JSON.parse(data);
				if (this.consumeAgentEvent(raw, onAgentEvent)) return;
				const parsed = raw as ApiChatCompletionStreamChunk;

				if (!firstValidChunkEmitted && parsed.object === 'chat.completion.chunk') {
					firstValidChunkEmitted = true;
					onFirstValidChunk?.();
				}

				const content = parsed.choices[0]?.delta?.content;
				const reasoningContent = parsed.choices[0]?.delta?.reasoning_content;
				const deltaToolCalls = parsed.choices[0]?.delta?.tool_calls;
				if (deltaToolCalls?.length) collectedToolCalls.push(...deltaToolCalls);
				const timings = parsed.timings;
				const promptProgress = parsed.prompt_progress;

				const chunkModel = this.extractModelName(parsed);
				if (chunkModel && !modelEmitted) {
					modelEmitted = true;
					onModel?.(chunkModel);
				}

				if (timings || promptProgress) {
					this.updateProcessingState(timings, promptProgress, conversationId);
					if (timings) lastTimings = timings;
				}

				if (content) {
					hasReceivedData = true;
					aggregatedContent += content;
					if (!abortSignal?.aborted) onChunk?.(content);
				}

				if (reasoningContent) {
					hasReceivedData = true;
					fullReasoningContent += reasoningContent;
					if (!abortSignal?.aborted) onReasoningChunk?.(reasoningContent);
				}
			} catch (e) {
				console.error('Error parsing JSON chunk:', e);
			}
		};

		try {
			const status = await invoke<number>('stream_chat', {
				streamId,
				url: absoluteUrl,
				headers,
				body,
				onChunk: channel
			});

			abortSignal?.removeEventListener('abort', onAbort);

			if (abortSignal?.aborted) {
				const error = new Error('Request aborted');
				error.name = 'AbortError';
				throw error;
			}

			if (status < 200 || status >= 300) {
				const error = new Error(`Server error (${status})`);
				error.name = 'HttpError';
				throw error;
			}

			if (!hasReceivedData && aggregatedContent.length === 0) {
				throw new Error('No response received from server. Please try again.');
			}

			onComplete?.(
				aggregatedContent,
				fullReasoningContent || undefined,
				lastTimings,
				collectedToolCalls.length > 0 ? collectedToolCalls : undefined
			);
		} catch (error) {
			abortSignal?.removeEventListener('abort', onAbort);

			if (abortSignal?.aborted) {
				const abortError = new Error('Request aborted');
				abortError.name = 'AbortError';
				throw abortError;
			}

			const msg =
				typeof error === 'string'
					? error
					: error instanceof Error
						? error.message
						: 'Unable to connect to server';
			const err = new Error(msg);
			err.name = error instanceof Error ? error.name : 'NetworkError';
			throw err;
		}
	}

	/**
	 * Handles non-streaming response from the chat completion API.
	 * Parses the JSON response and extracts the generated content.
	 *
	 * @param response - The fetch Response object containing the JSON data
	 * @param onComplete - Optional callback invoked when response is successfully parsed
	 * @param onError - Optional callback invoked if an error occurs during parsing
	 * @returns {Promise<string>} Promise that resolves to the generated content string
	 * @throws {Error} if the response cannot be parsed or is malformed
	 */
	private async handleNonStreamResponse(
		response: Response,
		onComplete?: (
			response: string,
			reasoningContent?: string,
			timings?: ChatMessageTimings,
			toolCalls?: DatabaseMessageToolCall[]
		) => void,
		onModel?: (model: string) => void
	): Promise<string> {
		try {
			const responseText = await response.text();

			if (!responseText.trim()) {
				const noResponseError = new Error('No response received from server. Please try again.');
				throw noResponseError;
			}

			const data: ApiChatCompletionResponse = JSON.parse(responseText);

			const responseModel = this.extractModelName(data);
			if (responseModel) {
				onModel?.(responseModel);
			}

			const content = data.choices[0]?.message?.content || '';
			const reasoningContent = data.choices[0]?.message?.reasoning_content;

			if (reasoningContent) {
				console.log('Full reasoning content:', reasoningContent);
			}

			if (!content.trim()) {
				const noResponseError = new Error('No response received from server. Please try again.');
				throw noResponseError;
			}

			onComplete?.(content, reasoningContent, undefined, data.choices[0]?.message?.tool_calls);

			return content;
		} catch (error) {
			throw error instanceof Error ? error : new Error('Parse error');
		}
	}

	/**
	 * Converts a database message with attachments to API chat message format.
	 * Processes various attachment types (images, text files, PDFs) and formats them
	 * as content parts suitable for the chat completion API.
	 *
	 * @param message - Database message object with optional extra attachments
	 * @param message.content - The text content of the message
	 * @param message.role - The role of the message sender (user, assistant, system)
	 * @param message.extra - Optional array of message attachments (images, files, etc.)
	 * @returns {ApiChatMessageData} object formatted for the chat completion API
	 * @static
	 */
	static convertMessageToChatServiceData(
		message: DatabaseMessage & { extra?: DatabaseMessageExtra[] }
	): ApiChatMessageData {
		if (!message.extra || message.extra.length === 0) {
			return {
				role: message.role as 'user' | 'assistant' | 'system',
				content: message.content
			};
		}

		const contentParts: ApiChatMessageContentPart[] = [];

		if (message.content) {
			contentParts.push({
				type: 'text',
				text: message.content
			});
		}

		const imageFiles = message.extra.filter(
			(extra: DatabaseMessageExtra): extra is DatabaseMessageExtraImageFile =>
				extra.type === 'imageFile'
		);

		for (const image of imageFiles) {
			contentParts.push({
				type: 'image_url',
				image_url: { url: image.base64Url }
			});
		}

		const textFiles = message.extra.filter(
			(extra: DatabaseMessageExtra): extra is DatabaseMessageExtraTextFile =>
				extra.type === 'textFile'
		);

		for (const textFile of textFiles) {
			contentParts.push({
				type: 'text',
				text: `\n\n--- File: ${textFile.name} ---\n${textFile.content}`
			});
		}

		// Handle legacy 'context' type from old webui (pasted content)
		const legacyContextFiles = message.extra.filter(
			(extra: DatabaseMessageExtra): extra is DatabaseMessageExtraLegacyContext =>
				extra.type === 'context'
		);

		for (const legacyContextFile of legacyContextFiles) {
			contentParts.push({
				type: 'text',
				text: `\n\n--- File: ${legacyContextFile.name} ---\n${legacyContextFile.content}`
			});
		}

		const audioFiles = message.extra.filter(
			(extra: DatabaseMessageExtra): extra is DatabaseMessageExtraAudioFile =>
				extra.type === 'audioFile'
		);

		for (const audio of audioFiles) {
			contentParts.push({
				type: 'input_audio',
				input_audio: {
					data: audio.base64Data,
					format: audio.mimeType.includes('wav') ? 'wav' : 'mp3'
				}
			});
		}

		const pdfFiles = message.extra.filter(
			(extra: DatabaseMessageExtra): extra is DatabaseMessageExtraPdfFile =>
				extra.type === 'pdfFile'
		);

		for (const pdfFile of pdfFiles) {
			if (pdfFile.processedAsImages && pdfFile.images) {
				for (let i = 0; i < pdfFile.images.length; i++) {
					contentParts.push({
						type: 'image_url',
						image_url: { url: pdfFile.images[i] }
					});
				}
			} else {
				contentParts.push({
					type: 'text',
					text: `\n\n--- PDF File: ${pdfFile.name} ---\n${pdfFile.content}`
				});
			}
		}

		return {
			role: message.role as 'user' | 'assistant' | 'system',
			content: contentParts
		};
	}

	/**
	 * Get server properties - static method for API compatibility.
	 * Tries same-origin ./props first (main server on 8080); if 404, falls back to model API /api/props (8081).
	 */
	static async getServerProps(): Promise<ApiLlamaCppServerProps> {
		const currentConfig = config();
		const apiKey = currentConfig.apiKey?.toString().trim();
		const headers: HeadersInit = {
			'Content-Type': 'application/json',
			...(apiKey ? { Authorization: `Bearer ${apiKey}` } : {})
		};

		try {
			const response = await fetch(`${getServerBaseUrl()}/props`, { headers });
			if (response.ok) {
				return await response.json();
			}
			if (response.status === 404) {
				const fallback = await fetch(`${getModelApiBaseUrl()}/api/props`, { headers });
				if (fallback.ok) {
					return await fallback.json();
				}
			}
			throw new Error(`Failed to fetch server props: ${response.status}`);
		} catch (error) {
			if (error instanceof SyntaxError) {
				console.error('Malformed JSON from server props:', error);
				throw error;
			}
			// Network/CORS error — fall back to Model API (has Access-Control-Allow-Origin: *)
			try {
				const mapiBase = getModelApiBaseUrl() || `http://127.0.0.1:${(window as any).__DELTA_MODEL_API_PORT__ ?? 8081}`;
				const fallback = await fetch(`${mapiBase}/api/props`, { headers });
				if (fallback.ok) {
					return await fallback.json();
				}
			} catch {}
			console.error('Error fetching server props:', error);
			throw error;
		}
	}

	/**
	 * Aborts any ongoing chat completion request.
	 * Cancels the current request and cleans up the abort controller.
	 *
	 * @public
	 */
	public abort(conversationId?: string): void {
		if (conversationId) {
			const abortController = this.abortControllers.get(conversationId);
			if (abortController) {
				abortController.abort();
				this.abortControllers.delete(conversationId);
			}
		} else {
			for (const controller of this.abortControllers.values()) {
				controller.abort();
			}
			this.abortControllers.clear();
		}
	}

	/**
	 * Injects a system message at the beginning of the conversation if configured in settings.
	 * Checks for existing system messages to avoid duplication and retrieves the system message
	 * from the current configuration settings.
	 *
	 * @param messages - Array of chat messages to process
	 * @returns Array of messages with system message injected at the beginning if configured
	 * @private
	 */
	private injectSystemMessage(messages: ApiChatMessageData[]): ApiChatMessageData[] {
		const currentConfig = config();
		const parts: string[] = [];

		const systemMessage = currentConfig.systemMessage?.toString().trim();
		if (systemMessage) parts.push(systemMessage);

		// Onboarding preferences. Emitted only when set, so an untouched install is unchanged.
		const userName = currentConfig.userName?.toString().trim();
		if (userName) {
			parts.push(
				`The user's name is ${userName}. Address them by name naturally, not in every sentence.`
			);
		}

		const replyStyle = currentConfig.replyStyle?.toString();
		if (replyStyle === 'concise') {
			parts.push('Keep answers short and to the point. Skip preamble.');
		} else if (replyStyle === 'detailed') {
			parts.push('Give thorough answers with the reasoning and relevant context included.');
		}

		if (parts.length === 0) {
			return messages;
		}

		const content = parts.join('\n\n');

		if (messages.length > 0 && messages[0].role === 'system') {
			if (messages[0].content !== content) {
				const updatedMessages = [...messages];
				updatedMessages[0] = {
					role: 'system',
					content
				};
				return updatedMessages;
			}

			return messages;
		}

		const systemMsg: ApiChatMessageData = {
			role: 'system',
			content
		};

		return [systemMsg, ...messages];
	}

	/**
	 * Ensures messages follow strict user/assistant alternation after an optional system message.
	 * Merges consecutive same-role messages so chat templates (e.g. Jinja) that require alternation don't error.
	 */
	private static ensureAlternatingRoles(messages: ApiChatMessageData[]): ApiChatMessageData[] {
		if (messages.length <= 1) return messages;

		const result: ApiChatMessageData[] = [];
		let i = 0;

		// Keep at most one system message at the start
		if (messages[0].role === 'system') {
			result.push(messages[0]);
			i = 1;
		}

		// Merge consecutive same-role messages so we get user -> assistant -> user -> assistant...
		// Tool turns are left alone: each tool result answers one call id, and an assistant turn
		// carrying tool_calls must stay its own message.
		while (i < messages.length) {
			const msg = messages[i];
			// Skip empty system messages in the middle (shouldn't happen after injectSystemMessage)
			if (msg.role === 'system') {
				i++;
				continue;
			}
			const last = result[result.length - 1];
			const isToolTurn = msg.role === 'tool' || !!msg.tool_calls;
			const lastIsToolTurn = !!last && (last.role === 'tool' || !!last.tool_calls);
			if (last && last.role === msg.role && !isToolTurn && !lastIsToolTurn) {
				// Merge content into last
				last.content = ChatService.mergeContent(last.content, msg.content);
			} else {
				result.push({ ...msg });
			}
			i++;
		}

		return result;
	}

	private static mergeContent(
		a: string | ApiChatMessageContentPart[],
		b: string | ApiChatMessageContentPart[]
	): string | ApiChatMessageContentPart[] {
		const toParts = (c: string | ApiChatMessageContentPart[]): ApiChatMessageContentPart[] =>
			typeof c === 'string' ? (c.trim() ? [{ type: 'text' as const, text: c }] : []) : c;
		const partsA = toParts(a);
		const partsB = toParts(b);
		if (partsA.length === 0) return b;
		if (partsB.length === 0) return a;
		return [...partsA, { type: 'text' as const, text: '\n\n' }, ...partsB];
	}

	/**
	 * Parses error response and creates appropriate error with context information
	 * @param response - HTTP response object
	 * @returns Promise<Error> - Parsed error with context info if available
	 */
	private async parseErrorResponse(response: Response): Promise<Error> {
		try {
			const errorText = await response.text();
			const errorData: ApiErrorResponse = JSON.parse(errorText);

			const message = errorData.error?.message || 'Unknown server error';
			const error = new Error(message);
			error.name = response.status === 400 ? 'ServerError' : 'HttpError';

			return error;
		} catch {
			const fallback = new Error(`Server error (${response.status}): ${response.statusText}`);
			fallback.name = 'HttpError';
			return fallback;
		}
	}

	/**
	 * Delta's harness frames ride the same SSE stream as the OpenAI chunks but have no `choices`,
	 * so they must be claimed before the chunk parsing runs -- otherwise reading `choices[0]`
	 * throws and the event is lost to the catch block.
	 *
	 * @returns true when the frame was a harness event and should not be parsed as a chunk.
	 */
	private consumeAgentEvent(parsed: unknown, onAgentEvent?: (event: AgentEvent) => void): boolean {
		if (!parsed || typeof parsed !== 'object') return false;
		const frame = parsed as { object?: unknown; event?: unknown; data?: unknown };
		if (frame.object !== 'delta.agent.event') return false;
		if (typeof frame.event !== 'string') return true;

		onAgentEvent?.({
			object: 'delta.agent.event',
			event: frame.event as AgentEventName,
			data: (frame.data ?? {}) as AgentEvent['data']
		});
		return true;
	}

	private extractModelName(data: unknown): string | undefined {
		const asRecord = (value: unknown): Record<string, unknown> | undefined => {
			return typeof value === 'object' && value !== null
				? (value as Record<string, unknown>)
				: undefined;
		};

		const getTrimmedString = (value: unknown): string | undefined => {
			return typeof value === 'string' && value.trim() ? value.trim() : undefined;
		};

		const root = asRecord(data);
		if (!root) return undefined;

		// 1) root (some implementations provide `model` at the top level)
		const rootModel = getTrimmedString(root.model);
		if (rootModel) return rootModel;

		// 2) streaming choice (delta) or final response (message)
		const firstChoice = Array.isArray(root.choices) ? asRecord(root.choices[0]) : undefined;
		if (!firstChoice) return undefined;

		// priority: delta.model (first chunk) else message.model (final response)
		const deltaModel = getTrimmedString(asRecord(firstChoice.delta)?.model);
		if (deltaModel) return deltaModel;

		const messageModel = getTrimmedString(asRecord(firstChoice.message)?.model);
		if (messageModel) return messageModel;

		// avoid guessing from non-standard locations (metadata, etc.)
		return undefined;
	}

	private updateProcessingState(
		timings?: ChatMessageTimings,
		promptProgress?: ChatMessagePromptProgress,
		conversationId?: string
	): void {
		const tokensPerSecond =
			timings?.predicted_ms && timings?.predicted_n
				? (timings.predicted_n / timings.predicted_ms) * 1000
				: 0;

		slotsService
			.updateFromTimingData(
				{
					prompt_n: timings?.prompt_n || 0,
					predicted_n: timings?.predicted_n || 0,
					predicted_per_second: tokensPerSecond,
					predicted_ms: timings?.predicted_ms || 0,
					cache_n: timings?.cache_n || 0,
					prompt_progress: promptProgress
				},
				conversationId
			)
			.catch((error) => {
				console.warn('Failed to update processing state:', error);
			});
	}
}

export const chatService = new ChatService();
