import type { Note } from '../+page';

export const load = async ({ fetch, params }: { fetch: typeof globalThis.fetch; params: { id: string } }) => {
  const res = await fetch(`/api/notes/${params.id}`);
  if (!res.ok) {
    return { note: null as Note | null, error: 'Note not found' };
  }
  const note = (await res.json()) as Note;
  return { note };
};
