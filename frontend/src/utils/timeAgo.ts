/**
 * Formats a timestamp into a human-readable "time ago" string
 * @param timestampMs - Timestamp in milliseconds
 * @returns Formatted string (e.g., "5 seconds ago", "2 minutes ago")
 */
export function timeAgo(timestampMs: number): string {
  const now = Date.now();
  const diffMs = now - timestampMs;
  const diffSec = Math.floor(diffMs / 1000);

  if (diffSec < 60) return `${diffSec} second${diffSec !== 1 ? "s" : ""} ago`;

  const diffMin = Math.floor(diffSec / 60);
  if (diffMin < 60) return `${diffMin} minute${diffMin !== 1 ? "s" : ""} ago`;

  const diffHr = Math.floor(diffMin / 60);
  if (diffHr < 24) return `${diffHr} hour${diffHr !== 1 ? "s" : ""} ago`;

  const diffDays = Math.floor(diffHr / 24);
  return `${diffDays} day${diffDays !== 1 ? "s" : ""} ago`;
}

