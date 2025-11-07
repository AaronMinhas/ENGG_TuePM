/**
 * Check if test/dummy mode is enabled
 * Enabled via URL parameter: ?test=true or ?testMode=true
 */
export function isTestMode(): boolean {
  const urlParams = new URLSearchParams(window.location.search);
  return urlParams.get("test") === "true" || urlParams.get("testMode") === "true";
}

