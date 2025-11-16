import * as universal from '../entries/pages/chat/_id_/_page.ts.js';

export const index = 3;
let component_cache;
export const component = async () => component_cache ??= (await import('../entries/pages/chat/_id_/_page.svelte.js')).default;
export { universal };
export const universal_id = "src/routes/chat/[id]/+page.ts";
export const imports = [];
export const stylesheets = [];
export const fonts = [];
