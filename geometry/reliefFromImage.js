import { decode } from 'fast-png';
import { relief } from './relief.js';
import { Points } from './point.js';

export const reliefFromImage = (
  assets,
  buffer,
  {
    rows = 10,
    cols = 10,
    mapping = 'planar',
    subdivisions = 4,
    closedU = false,
    closedV = false,
    targetEdgeLength = 0,
    width = 10,
    height = 10,
    depth = 1,
  } = {}
) => {
  const png = decode(buffer);
  const { width: imgWidth, height: imgHeight, data: pixels } = png;

  const points = [];
  for (let j = 0; j < imgHeight; j++) {
    for (let i = 0; i < imgWidth; i++) {
      const idx = (j * imgWidth + i) * png.channels;
      // Simple grayscale average
      const r = pixels[idx];
      const g = pixels[idx + 1] ?? r;
      const b = pixels[idx + 2] ?? r;
      const brightness = (r + g + b) / 3 / 255;

      const x = (i / (imgWidth - 1) - 0.5) * width;
      const y = (0.5 - j / (imgHeight - 1)) * height;
      const z = brightness * depth;
      points.push([x, y, z]);
    }
  }

  return relief(assets, Points(assets, points), {
    rows,
    cols,
    mapping,
    subdivisions,
    closedU,
    closedV,
    targetEdgeLength,
  });
};
