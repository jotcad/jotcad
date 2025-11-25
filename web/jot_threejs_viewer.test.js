import { decodeTf } from './jot_threejs_viewer.js';
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';

describe('decodeTf', () => {
  it('should correctly decode a translate command', () => {
    const tf = 't 1 2 3';
    const matrix = decodeTf(tf);
    const expected = new THREE.Matrix4().makeTranslation(1, 2, 3);
    assert.deepStrictEqual(matrix.elements, expected.elements);
  });

  it('should correctly decode a scale command', () => {
    const tf = 's 2 3 4';
    const matrix = decodeTf(tf);
    const expected = new THREE.Matrix4().makeScale(2, 3, 4);
    assert.deepStrictEqual(matrix.elements, expected.elements);
  });

  it('should correctly decode a sequence of commands', () => {
    const tf = ['t 1 2 3', 's 2 3 4'];
    const matrix = decodeTf(tf);
    const t = new THREE.Matrix4().makeTranslation(1, 2, 3);
    const s = new THREE.Matrix4().makeScale(2, 3, 4);
    const expected = t.multiply(s);
    assert.deepStrictEqual(matrix.elements, expected.elements);
  });
});
