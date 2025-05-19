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
  FS.mkdir('/assets/text');
  FS.mount(FS.filesystems.NODEFS, { root: '/tmp/assets' }, '/assets');
  assertCgalIsReady();
};

const getCgal = () => cgal;

initCgal();

export { FS, cgal, cgalIsReady, getCgal };
