export const DecodeInexactGeometryText = (text) => {
  const vertices = [];
  const segments = [];
  const triangles = [];
  const faces = [];

  for (const line of text.split('\n')) {
    const pieces = line.split(' ');
    const code = pieces.shift();
    switch (code) {
      case 'v':
        while (pieces.length >= 3) {
          pieces.shift();
          pieces.shift();
          pieces.shift();
          vertices.push([
            parseFloat(pieces.shift()),
            parseFloat(pieces.shift()),
            parseFloat(pieces.shift()),
          ]);
        }
        break;
      case 'p':
        while (pieces.length >= 1) {
          points.push(parseInt(pieces.shift()));
        }
        break;
      case 's':
        while (pieces.length >= 2) {
          const source = parseInt(pieces.shift());
          const target = parseInt(pieces.shift());
          segments.push([source, target]);
        }
        break;
      case 't': {
        const triangle = [];
        while (pieces.length >= 1) {
          triangle.push(parseInt(pieces.shift()));
        }
        triangles.push(triangle);
        break;
      }
      case 'f': {
        const face = [];
        while (pieces.length >= 1) {
          face.push(parseInt(pieces.shift()));
        }
        faces.push([face]);
        break;
      }
      case 'h': {
        const hole = [];
        while (pieces.length >= 1) {
          hole.push(parseInt(pieces.shift()));
        }
        faces.at(-1).push(hole);
        break;
      }
    }
  }
  return {
    vertices,
    segments,
    triangles,
    faces,
  };
};

const round = (value, give) => Math.round(value / give) * give;

export const EncodeInexactGeometryText = ({
  vertices = [],
  segments = [],
  triangles = [],
  faces = [],
}) => {
  const lines = [];
  for (const vertex of vertices) {
    lines.push(
      `v ${vertex[0]} ${vertex[1]} ${vertex[2]} ${vertex[0]} ${vertex[1]} ${vertex[2]}\n`
    );
  }
  if (segments.length > 0) {
    const pieces = [];
    for (const segment of segments) {
      pieces.push(`${segment[0]} ${segment[1]}`);
    }
    lines.push(`s ${pieces.join(' ')}\n`);
  }
  for (const triangle of triangles) {
    lines.push(`t ${triangle[0]} ${triangle[1]} ${triangle[2]}\n`);
  }
  for (const [face, holes = []] of faces) {
    lines.push(`f ${face.join(' ')}\n`);
    for (const hole of holes) {
      lines.push(`h ${hole.join(' ')}\n`);
    }
  }
  return lines.join('');
};
