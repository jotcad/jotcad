import { cut3 } from './cut.js';
import { makeGroup } from './shape.js';

export const disjoint = (assets, shapes) => {
  const results = [];
  for (let i = 0; i < shapes.length; i++) {
    const tools = shapes.slice(i + 1);
    if (tools.length > 0) {
      results.push(cut3(assets, shapes[i], tools));
    } else {
      results.push(shapes[i]);
    }
  }
  return makeGroup(results);
};
