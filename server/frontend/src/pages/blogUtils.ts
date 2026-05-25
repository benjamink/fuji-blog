/** Strip markdown syntax and return plain-text excerpt. */
export function getExcerpt(markdown: string, maxLen = 260): string {
  const plain = markdown
    .replace(/^#{1,6}\s+.*$/gm, '')               // headings
    .replace(/```[\s\S]*?```/g, '')                // fenced code blocks
    .replace(/`[^`]+`/g, '')                       // inline code
    .replace(/!?\[([^\]]*)\]\([^)]*\)/g, '$1')    // links / images → text
    .replace(/[*_~]{1,3}([^*_~\n]+)[*_~]{1,3}/g, '$1') // bold / italic
    .replace(/^\s*[-*+>]\s+/gm, '')               // list bullets, blockquotes
    .replace(/\n{2,}/g, ' ')
    .replace(/\n/g, ' ')
    .replace(/\s{2,}/g, ' ')
    .trim()
  return plain.length > maxLen ? plain.slice(0, maxLen).trimEnd() + '…' : plain
}

/** Format an ISO date string as a readable date, e.g. "May 24, 2026". */
export function formatDate(iso: string): string {
  return new Date(iso).toLocaleDateString('en-US', {
    year: 'numeric',
    month: 'long',
    day: 'numeric',
  })
}
