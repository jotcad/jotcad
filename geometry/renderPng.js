// import UPNG from 'upng-js';
import { encode } from 'fast-png';
import gl from 'gl';
import { staticDisplay } from './threejs.js';

export const isNode =
  typeof process !== 'undefined' &&
  process.versions != null &&
  process.versions.node != null;

const extractPixels = (context) => {
  const width = context.drawingBufferWidth;
  const height = context.drawingBufferHeight;
  const frameBufferPixels = new Uint8Array(width * height * 4);
  context.readPixels(
    0,
    0,
    width,
    height,
    context.RGBA,
    context.UNSIGNED_BYTE,
    frameBufferPixels
  );
  // The framebuffer coordinate space has (0, 0) in the bottom left, whereas images usually
  // have (0, 0) at the top left. Vertical flipping follows:
  const pixels = new Uint8Array(width * height * 4);
  for (let fbRow = 0; fbRow < height; fbRow += 1) {
    let rowData = frameBufferPixels.subarray(
      fbRow * width * 4,
      (fbRow + 1) * width * 4
    );
    let imgRow = height - fbRow - 1;
    pixels.set(rowData, imgRow * width * 4);
  }
  return { width, height, pixels };
};

export const renderPng = async (
  assets,
  shape,
  {
    view = {},
    withAxes = false,
    withGrid = false,
    definitions,
    width,
    height,
  } = {}
) => {
  let context;
  let canvas;

  if (isNode) {
    canvas = {
      width,
      height,
      addEventListener: (event) => {},
      removeEventListener: (event) => {},
      getContext: () => context,
    };
    // But this is not available in a web-worker.
    context = gl(width, height, { canvas, preserveDrawingBuffer: true });
  }

  const target = [0, 0, 0];
  const position = [0, 0, 20];
  const up = [0, 0.0001, 1];

  const { renderer } = await staticDisplay({
    assets,
    view: { target, position, up, ...view },
    canvas,
    context,
    definitions,
    shape,
    withAxes,
    withGrid,
    width,
    height,
  });
  const { pixels } = extractPixels(renderer.getContext());
  return encode({ width, height, data: pixels, channels: 4 });
};
