import { getDefaultContext } from '@emnapi/runtime';

let assertCgalIsReady;
const cgalIsReady = new Promise((resolve, reject) => {
  assertCgalIsReady = resolve;
});

let cgal;

const initCgal = async () => {
  const module = await Module({
    /* Emscripten module init options */
  });
  cgal = module.emnapiInit({ context: getDefaultContext() });
  assertCgalIsReady();
};

const getCgal = () => cgal;

initCgal();

export { cgal, cgalIsReady, initCgal, getCgal };
