import { Op } from '@jotcad/op';
import { Shape } from '@jotcad/geometry';

export const registerOp = ({ name, effect = false, spec, args, code }) => {
  const cachingCode = async (id, session, ...args) => {
    const assets = session.assets;
    const result = assets.getResult(id);
    if (result && effect === false) {
      const { value } = result;
      return Shape.fromPlain(value);
    } else {
      // TODO: Figure out side-effect emission.
      const value = await code(id, session, ...args);
      assets.setResult(id, { value });
      return value;
    }
  };
  return Op.registerOp({ name, spec, args, code: cachingCode });
};
