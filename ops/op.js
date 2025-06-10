import { Op } from '@jotcad/op';

export const registerOp = ({ name, spec, args, code }) => {
  const cachingCode = async (id, assets, ...args) => {
    const result = assets.getResult(id);
    if (result) {
      return result.value;
    } else {
      // TODO: Figure out side-effect emission.
      const value = await code(id, assets, ...args);
      assets.setResult(id, { value });
      return value;
    }
  };
  return Op.registerOp({ name, spec, args, code: cachingCode });
};
