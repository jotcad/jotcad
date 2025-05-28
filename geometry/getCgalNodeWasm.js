import Module from './wasm.cjs';
import { getDefaultContext } from '@emnapi/runtime';

let assertCgalIsReady;
const cgalIsReady = new Promise((resolve, reject) => {
  assertCgalIsReady = resolve;
});

let cgal;
let FS;

const initCgal = async () => {
  const module = await Module({
    /* Emscripten module init options */
  });
  cgal = module.emnapiInit({ context: getDefaultContext() });
  FS = module.FS;
  FS.mkdir('/assets');
  FS.mount(FS.filesystems.NODEFS, { root: '.' }, '/assets');
  try {
    FS.mkdir('/assets/text');
  } catch (e) {
    if (e.errno !== 20) {
      // (EEXIST)
      throw e; // Re-throw other errors
    }
  }
  assertCgalIsReady();
};

const getCgal = () => cgal;

initCgal();

export { FS, cgal, cgalIsReady, getCgal };
