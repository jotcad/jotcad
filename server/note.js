import { Op } from '@jotcad/op';

export const note = Op.registerOp(
  'note',
  ['shape', ['string'], 'shape'],
  async (assets, input, note) => {
    console.log(note);
    return input;
  }
);
