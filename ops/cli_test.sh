#!/usr/bin/env -S node cli

Box2([0, 5], [0, 5])
  .fill()
  .extrude(z(5), z(0))
  .color('blue')
  .cut([Box3([2, 5], [2, 5], [2, 5])])
  .and(Arc2(20, { give: 1 }))
  .png('box.png', [15, 15, 15])
  .stl('box.stl');
