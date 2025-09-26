import { Op } from '@jotcad/op';
import { Shape } from '@jotcad/geometry';

export const registerOp = ({ name, effect = false, spec, args, code }) => {
  const cachingCode = async (id, assets, ...args) => {
    const result = assets.getResult(id);
    if (result && effect === false) {
      const { value } = result;
      const walk = (shape) => {
        Object.setPrototypeOf(shape, Shape.prototype);
        if (shape.shapes) {
          for (const element of shape.shapes) {
            walk(element);
          }
        }
      };
      walk(value);
      return value;
    } else {
      // TODO: Figure out side-effect emission.
      const value = await code(id, assets, ...args);
      assets.setResult(id, { value });
      return value;
    }
  };
  return Op.registerOp({ name, spec, args, code: cachingCode });
};
