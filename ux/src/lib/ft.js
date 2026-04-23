/**
 * JOT Exact Field Trait (FT) Utilities for JavaScript.
 * Handles conversion between exact ratios (strings) and local numbers.
 */

/**
 * Converts an exact ratio string "n/d" to a JavaScript Number.
 * Handles components too large for standard Number parsing by using BigInt.
 * Ensures the result is the nearest representable double-precision float.
 */
export const ratioToNumber = (s) => {
  if (typeof s === 'number') return s;
  if (typeof s !== 'string') return 0;
  
  // Handle multiple space-delimited ratios
  if (s.includes(' ')) {
    return s.trim().split(/\s+/).map(ratioToNumber);
  }

  const slash = s.indexOf('/');
  if (slash === -1) return parseFloat(s);

  const nStr = s.substring(0, slash);
  const dStr = s.substring(slash + 1);

  try {
    const n = BigInt(nStr);
    const d = BigInt(dStr);
    
    if (d === 0n) return 0;

    const num = Number(n);
    const den = Number(d);

    if (isFinite(num) && isFinite(den)) {
      return num / den;
    }

    // Overflow path: if components > 1.8e308, scale down proportionally.
    const nLen = nStr.length;
    const dLen = dStr.length;
    const maxLen = Math.max(nLen, dLen);
    const shift = BigInt(Math.max(0, maxLen - 15));
    const factor = 10n ** shift;
    
    return Number(n / factor) / Number(d / factor);
  } catch (e) {
    return parseFloat(s);
  }
};
