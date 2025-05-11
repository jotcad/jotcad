#!/usr/bin/env -S node cli

Box2([0, 5], [0, 5])
  .fill()
  .extrude(z(5))
  .color('blue')
  .cut([Box3([2, 5], [2, 5], [2, 5])])
  .and(Arc2(20, { give: 1 }).color('red'))
  .save('cli_test.json')
  .png('cli_test.png', [25, 25, 25])
  .stl('cli_test.stl');
