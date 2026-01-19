import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const sweep = (
  assets,
  profiles,
  path,
  {
    closedPath = false,
    closedProfile = false,
    strategy = 0,
    solid = true,
    iota = -1,
    minTurnRadius = 0.01,
  } = {}
) => {
  const target = makeShape();
  cgal.Sweep(
    assets,
    target,
    [].concat(profiles),
    path,
    closedPath,
    closedProfile,
    strategy,
    solid,
    iota,
    minTurnRadius
  );
  return target;
};
