#!/usr/bin/env -S node cli

Box3([0, 1], [0, 1], [0, 1])
  .x({ by: 2, lt: 10 })
  .y({ by: 2, lt: 10 })
  .z({ by: 2, lt: 10 })
  .view([35, 15, 25]);
