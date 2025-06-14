#!/usr/bin/env -S node cli

Box2([0, 5], [0, 5])
  .fill2()
  .extrude2(z(5))
  .view()
  .color('blue')
  .cut(Box3([2, 5], [2, 5], [2, 5]).color('green'))
  .join(Arc2(20, { give: 1 }).fill2().color('red').extrude2(z(1), z(-1)))
  .test('si', 'Self Intersection Detected')
  .save('cli_test.json')
  .png('cli_test.png', [25, 25, 25])
  .stl('cli_test.stl')
  .testPng('cli_test.png', [35, 15, 25])
  .view([35, 15, 25]);
