/**
 * Converts an exact ratio string "n/d" to a JavaScript Number.
 */
export const ratioToNumber = (s) => {
  if (typeof s === 'number') return s;
  if (typeof s !== 'string') return 0;
  
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
    if (isFinite(num) && isFinite(den)) return num / den;
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

/**
 * Decodes the JOT text format into a structured Geometry object.
 */
export const decodeGeometry = (text) => {
  const vertices = [], points = [], triangles = [], segments = [], faces = [];
  if (!text) return { vertices, points, triangles, segments, faces };
  const lines = text.split('\n');
  let i = 0;
  while (i < lines.length) {
    const line = lines[i].trim();
    if (!line) { i++; continue; }
    const pieces = line.split(/\s+/);
    const code = pieces.shift();
    if (code === 'V') {
      const count = parseInt(pieces[0]); i++;
      for (let j = 0; j < count && i < lines.length; j++, i++) {
        const vLine = lines[i].trim().split(/\s+/);
        if (vLine.length >= 3) vertices.push({
            x: ratioToNumber(vLine[0]), 
            y: ratioToNumber(vLine[1]), 
            z: ratioToNumber(vLine[2])
        });
      }
    } else if (code === 'F') {
      const count = parseInt(pieces[0]); i++;
      for (let j = 0; j < count && i < lines.length; j++, i++) {
        const fLine = lines[i].trim().split(/\s+/);
        if (fLine.length === 0) continue;
        const numLoops = parseInt(fLine.shift() || '1');
        const loops = [];
        for (let l = 0; l < numLoops; l++) {
           const loopLen = parseInt(fLine.shift() || '0');
           const loop = [];
           for (let k = 0; k < loopLen; k++) {
              const idx = parseInt(fLine.shift() || '-1');
              if (!isNaN(idx) && idx >= 0) loop.push(idx);
           }
           if (loop.length > 0) loops.push(loop);
        }
        if (loops.length > 0) faces.push({ loops });
      }
    } else if (code === 'P') {
      const count = parseInt(pieces[0]); i++;
      if (count > 0 && i < lines.length) {
        const pLine = lines[i].trim().split(/\s+/);
        for (let idxStr of pLine) { const idx = parseInt(idxStr); if (!isNaN(idx)) points.push(idx); }
        i++;
      }
    } else if (code === 'S') {
      const count = parseInt(pieces[0]); i++;
      for (let j = 0; j < count && i < lines.length; j++, i++) {
        const sLine = lines[i].trim().split(/\s+/);
        if (sLine.length >= 2) segments.push({ a: parseInt(sLine[0]), b: parseInt(sLine[1]) });
      }
    } else if (code === 'T') {
      const count = parseInt(pieces[0]); i++;
      for (let j = 0; j < count && i < lines.length; j++, i++) {
        const tLine = lines[i].trim().split(/\s+/);
        if (tLine.length >= 3) triangles.push({ a: parseInt(tLine[0]), b: parseInt(tLine[1]), c: parseInt(tLine[2]) });
      }
    } else i++;
  }
  return { vertices, points, triangles, segments, faces };
};
