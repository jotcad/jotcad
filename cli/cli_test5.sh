#!/usr/bin/env -S node cli

Box2([0, 5], [0, 5])
  .fill2()
  .rz({ by: 1/16, lt: 1/4 })
  .extrude2(Z(1))
  .join()
  .absolute()
  .x(10)
  .set('id', 'one')
  .and(Box2(1, 1).color('green'))
  .at(get('id', 'one'), color('red').rz(1/8))
  .testPng('cli_test5.png')
  .view();
