export interface Note {
  id: string;
  title: string;
  content: string;
  folder: string;
  tags: string;
  pinned: boolean;
  created_at: string;
  updated_at: string;
}

export const load = async ({ fetch, url }: { fetch: typeof globalThis.fetch; url: URL }) => {
  const search = url.searchParams.get('search') || '';
  const folder = url.searchParams.get('folder') || '';
  const tags = url.searchParams.get('tags') || '';

  const params = new URLSearchParams();
  if (search) params.set('search', search);
  if (folder) params.set('folder', folder);
  if (tags) params.set('tags', tags);
  params.set('limit', '50');

  const res = await fetch(`/api/notes?${params.toString()}`);
  if (!res.ok) {
    return { notes: [] as Note[], error: 'Failed to load notes' };
  }
  const data = await res.json();
  return {
    notes: (data.notes || []) as Note[],
    search,
    folder,
    tags
  };
};
