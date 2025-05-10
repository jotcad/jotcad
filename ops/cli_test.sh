#!/usr/bin/env -S node cli

Box2([0, 0], [5, 5])
  .fill()
  .extrude(z(5), z(0))
  .color('blue')
  .cut([Box3([2, 2, 2], [5, 5, 5])])
  .png('box.png', [15, 15, 15])
  .stl('box.stl');
