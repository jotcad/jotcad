#!/usr/bin/env -S node cli

Box2([0, 0], [5, 10])
  .fill()
  .extrude(z(2), z(0))
  .color('blue')
  .png('box.png', [0, 0, 10])
  .stl('box.stl');
