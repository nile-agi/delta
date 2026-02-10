
// this file is generated — do not edit it


declare module "svelte/elements" {
	export interface HTMLAttributes<T> {
		'data-sveltekit-keepfocus'?: true | '' | 'off' | undefined | null;
		'data-sveltekit-noscroll'?: true | '' | 'off' | undefined | null;
		'data-sveltekit-preload-code'?:
			| true
			| ''
			| 'eager'
			| 'viewport'
			| 'hover'
			| 'tap'
			| 'off'
			| undefined
			| null;
		'data-sveltekit-preload-data'?: true | '' | 'hover' | 'tap' | 'off' | undefined | null;
		'data-sveltekit-reload'?: true | '' | 'off' | undefined | null;
		'data-sveltekit-replacestate'?: true | '' | 'off' | undefined | null;
	}
}

export {};


declare module "$app/types" {
	export interface AppTypes {
		RouteId(): "/" | "/chat" | "/chat/[id]";
		RouteParams(): {
			"/chat/[id]": { id: string }
		};
		LayoutParams(): {
			"/": { id?: string };
			"/chat": { id?: string };
			"/chat/[id]": { id: string }
		};
		Pathname(): "/" | "/chat" | "/chat/" | `/chat/${string}` & {} | `/chat/${string}/` & {};
		ResolvedPathname(): `${"" | `/${string}`}${ReturnType<AppTypes['Pathname']>}`;
		Asset(): "/GPT logo.png" | "/angleslim logo.webp" | "/deepseek logo.webp" | "/favicon.svg" | "/gemma logo.svg" | "/glm logo.svg" | "/glm.svg" | "/loading.html" | "/ministral logo.png" | "/nemotron logo.png" | "/qwen logo.jpeg" | string & {};
	}
}