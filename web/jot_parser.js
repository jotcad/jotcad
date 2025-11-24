// web/jot_parser.js

export const DecodeInexactGeometryText = (text) => {
  const vertices = [];
  const segments = [];
  const triangles = [];
  const faces = [];

  const lines = text.split('\n');
  for (const line of lines) {
    const trimmedLine = line.trim();
    if (trimmedLine.length === 0) {
      continue;
    }
    const parts = trimmedLine.split(' ');
    const command = parts.shift();

    switch (command) {
      case 'v':
        vertices.push(parts.map(parseFloat));
        break;
      case 's':
        segments.push(parts.map((s) => parseInt(s, 10)));
        break;
      case 't':
        triangles.push(parts.map((s) => parseInt(s, 10)));
        break;
      case 'f': {
        const faceAndHoles = [];
        let remaining = parts.join(' ');
        while (remaining.length > 0) {
          const match = remaining.match(/^\s*\(([^)]*)\)\s*|^\s*([0-9]+)\s*/);
          if (match) {
            const [full, hole, point] = match;
            if (hole !== undefined) {
              faceAndHoles.push(
                hole
                  .trim()
                  .split(/\s+/)
                  .map((s) => parseInt(s, 10))
              );
            } else if (point !== undefined) {
              faceAndHoles.push(parseInt(point, 10));
            }
            remaining = remaining.substring(full.length);
          } else {
            break;
          }
        }
        const face = [];
        const holes = [];
        let current = face;
        for (const item of faceAndHoles) {
          if (Array.isArray(item)) {
            holes.push(item);
          } else {
            current.push(item);
          }
        }
        faces.push([face, ...holes]);
        break;
      }
      default:
        // Ignore other commands like 'V' (version) for now
        break;
    }
  }
  return { vertices, segments, triangles, faces };
};
