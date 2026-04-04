import { describe, it } from 'node:test';
import { toPdf } from './pdf.js';

import assert from 'node:assert/strict';
import { makeShape } from './shape.js';
import { withTestAssets } from './test_session_util.js';

describe('pdf', () => {
  it('should convert a triangle to a pdf', async () => {
    await withTestAssets(
      'should convert a triangle to a pdf',
      async (assets) => {
        const id = assets.textId(
          'V 3\nv 0 0 0 0 0 0\nv 10 0 0 10 0 0\nv 0 10 0 0 10 0\nT 1\nt 0 1 2\n'
        );
        const pdf = new TextDecoder('utf8').decode(
          toPdf(assets, makeShape({ geometry: id }))
        );
        assert.ok(pdf.includes('%PDF-1.5'));
        assert.ok(pdf.includes('f'));
        assert.ok(pdf.includes('endstream'));
      }
    );
  });

  it('should convert segments to a pdf', async () => {
    await withTestAssets('should convert segments to a pdf', async (assets) => {
      const id = assets.textId(
        'V 2\nv 0 0 0 0 0 0\nv 10 10 0 10 10 0\nS 1\ns 0 1\n'
      );
      const pdf = new TextDecoder('utf8').decode(
        toPdf(assets, makeShape({ geometry: id }))
      );
      assert.ok(pdf.includes('%PDF-1.5'));
      assert.ok(pdf.includes('m'));
      assert.ok(pdf.includes('l'));
      assert.ok(pdf.includes('S'));
      assert.ok(pdf.includes('endstream'));
    });
  });
});
