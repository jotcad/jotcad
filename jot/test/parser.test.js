import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotEvaluator } from '../src/evaluator.js';

test('JotCAD Parser: Basic Function Call', () => {
    const parser = new JotParser();
    const result = parser.parse('box(100)');
    
    assert.strictEqual(result.path, 'shape/box');
    assert.strictEqual(result.parameters.width, 100);
});

test('JotCAD Parser: Method Chaining', () => {
    const parser = new JotParser();
    const result = parser.parse('box(100).rx(0.1)');
    
    assert.strictEqual(result.path, 'op/rotateX');
    assert.strictEqual(result.parameters.turns, 0.1);
    assert.strictEqual(result.parameters.source.path, 'shape/box');
    assert.strictEqual(result.parameters.source.parameters.width, 100);
});

test('JotCAD Parser: Late-Bound Symbols', () => {
    const parser = new JotParser();
    const result = parser.parse('box(length)');
    
    assert.strictEqual(result.parameters.width.type, 'SYMBOL');
    assert.strictEqual(result.parameters.width.name, 'length');
});

test('JotCAD Evaluator: Symbol Resolution', () => {
    const parser = new JotParser();
    const evaluator = new JotEvaluator();
    
    const expression = parser.parse('box(length).rx(turn)');
    const resolved = evaluator.resolve(expression, { length: 50, turn: 0.25 });
    
    assert.strictEqual(resolved.path, 'op/rotateX');
    assert.strictEqual(resolved.parameters.turns, 0.25);
    assert.strictEqual(resolved.parameters.source.path, 'shape/box');
    assert.strictEqual(resolved.parameters.source.parameters.width, 50);
});

test('JotCAD Parser: Deep Method Chaining', () => {
    const parser = new JotParser();
    const result = parser.parse('box(100).rx(0.1).offset(5).outline()');
    
    assert.strictEqual(result.path, 'op/outline');
    assert.strictEqual(result.parameters.source.path, 'op/offset');
    assert.strictEqual(result.parameters.source.parameters.radius, 5);
    assert.strictEqual(result.parameters.source.parameters.source.path, 'op/rotateX');
});

test('JotCAD Parser: Array of Expressions', () => {
    const parser = new JotParser();
    const result = parser.parse('[box(10), arc(20).rx(0.5)]');
    
    assert.ok(Array.isArray(result));
    assert.strictEqual(result[0].path, 'shape/box');
    assert.strictEqual(result[1].path, 'op/rotateX');
});

test('JotCAD Parser: Complex Object Literals', () => {
    const parser = new JotParser();
    const result = parser.parse('tri({ points: [pt(0,0), pt(10,0), pt(5,10)], color: "red" })');
    
    assert.strictEqual(result.path, 'shape/triangle');
    assert.strictEqual(result.parameters.color, 'red');
    assert.ok(Array.isArray(result.parameters.points));
    assert.strictEqual(result.parameters.points[0].path, 'shape/point');
});

test('JotCAD Evaluator: Deep Symbol Resolution', () => {
    const parser = new JotParser();
    const evaluator = new JotEvaluator();
    
    const expression = parser.parse('box({ width: width }).rx(turn)');
    const params = { width: 50, turn: 0.1 };
    const resolved = evaluator.resolve(expression, params);
    
    assert.strictEqual(resolved.parameters.turns, 0.1);
    assert.strictEqual(resolved.parameters.source.parameters.width, 50);
});

test('JotCAD Evaluator: .spec() Canonicalization', () => {
    const parser = new JotParser();
    const evaluator = new JotEvaluator();
    
    const expression = 'box(length).spec({ length: { alias: ["L", "len"], default: 50 } })';
    const ast = parser.parse(expression);
    
    // 1. Resolve with alias
    const res1 = evaluator.resolve(ast, { L: 100 });
    assert.strictEqual(res1.parameters.width, 100);
    
    // 2. Resolve with default
    const res2 = evaluator.resolve(ast, {});
    assert.strictEqual(res2.parameters.width, 50);
    
    // 3. Resolve with canonical name
    const res3 = evaluator.resolve(ast, { length: 200 });
    assert.strictEqual(res3.parameters.width, 200);
});
