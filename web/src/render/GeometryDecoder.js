import * as THREE from 'three';

// Helper to evaluate fractions or ratio strings to floats
export function ratioToNumber(str) {
  if (str.includes('/')) {
    const parts = str.split('/');
    return parseFloat(parts[0]) / parseFloat(parts[1]);
  }
  return parseFloat(str);
}

// Decode custom .jot shape text representation format into vertices, triangles, and segments
export function decodeJotGeometry(text) {
  const vertices = [];
  const points = [];
  const triangles = [];
  const segments = [];
  const faces = [];
  
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
        if (vLine.length >= 3) {
          vertices.push([ratioToNumber(vLine[0]), ratioToNumber(vLine[1]), ratioToNumber(vLine[2])]);
        }
      }
    } else if (code === 'F') {
      const count = parseInt(pieces[0]); i++;
      for (let j = 0; j < count && i < lines.length; j++, i++) {
        const fLine = lines[i].trim().split(/\s+/);
        if (fLine.length === 0) continue;
        const numLoops = parseInt(fLine.shift() || '1');
        for (let l = 0; l < numLoops; l++) {
           const loopLen = parseInt(fLine.shift() || '0');
           const loop = [];
           for (let k = 0; k < loopLen; k++) {
              const idx = parseInt(fLine.shift() || '-1');
              if (!isNaN(idx) && idx >= 0) loop.push(idx);
           }
           if (loop.length > 0) {
              if (l === 0) faces.push([loop]);
              else faces.at(-1).push(loop);
           }
        }
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
        if (sLine.length >= 2) segments.push([parseInt(sLine[0]), parseInt(sLine[1])]);
      }
    } else if (code === 'T') {
      const count = parseInt(pieces[0]); i++;
      for (let j = 0; j < count && i < lines.length; j++, i++) {
        const tLine = lines[i].trim().split(/\s+/);
        if (tLine.length >= 3) triangles.push([parseInt(tLine[0]), parseInt(tLine[1]), parseInt(tLine[2])]);
      }
    } else {
      i++;
    }
  }
  return { vertices, points, triangles, segments, faces };
}
