// web/jot_parser.js

export const DecodeInexactGeometryText = (text) => {
  const vertices = [];
  const points = [];
  const segments = [];
  const triangles = [];
  const faces = [];

  const lines = text.split('\n');
  for (const line of lines) {
    const trimmedLine = line.trim();
    if (trimmedLine.length === 0) {
      continue;
    }
    const parts = trimmedLine.split(/\s+/);
    const command = parts.shift();

    switch (command) {
      case 'v':
        vertices.push(parts.map(parseFloat));
        break;
      case 'p':
        points.push(...parts.map((s) => parseInt(s, 10)));
        break;
      case 's': {
        const indices = parts.map((s) => parseInt(s, 10));
        // segments are stored as arrays of pairs or a single array of indices?
        // geometry.h Encode says: ss << " " << s << " " << t;
        // So it's a flat list of indices representing pairs.
        for (let i = 0; i < indices.length; i += 2) {
          segments.push([indices[i], indices[i + 1]]);
        }
        break;
      }
      case 't':
        triangles.push(parts.map((s) => parseInt(s, 10)));
        break;
      case 'f': {
        const face = parts.map((s) => parseInt(s, 10));
        faces.push([face]);
        break;
      }
      case 'h': {
        const hole = parts.map((s) => parseInt(s, 10));
        if (faces.length > 0) {
          faces[faces.length - 1].push(hole);
        }
        break;
      }
      default:
        // Ignore other commands
        break;
    }
  }
  return { vertices, points, segments, triangles, faces };
};
