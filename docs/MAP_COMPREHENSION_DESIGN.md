# JOT CAD Map Comprehension & Plural Move Design Specification

This document details the design for introducing **Map Comprehension** (`[range do param -> expr]`) syntax to the JOT language, supporting `jot:vec3s` vector sequences, and refactoring the `move` operator to accept coordinate sequences.

---

## 1. Syntax & Semantics

### Map Comprehension Syntax
The bracketed map comprehension generates a sequence of items (e.g. coordinates or dimensions) by applying a projection function to an iterable range:

```jot
[ <iterable_expr> do <parameter_name> -> <projection_expr> ]
```

* **Example (Vector Coordinates)**:
  ```jot
  coords = [0 .. 2.7 by 0.3 do $ -> [0, $, $]]
  ```
  Generates the list of 3D vectors:
  `[[0, 0, 0], [0, 0.3, 0.3], ..., [0, 2.7, 2.7]]`

* **Example (Asymmetric Box Offsets)**:
  ```jot
  boxes = [1 .. 5 do x -> Box(x, 10, 10).move([x * 2, 0, 0])]
  ```

### Plural Move Operator
The `move` operator is extended to accept a single coordinate vector (`jot:vec3`) or a sequence of coordinate vectors (`jot:vec3s`):

```jot
// 1. Single Vector Translation (Singular)
shape.move([1, 2, 3])

// 2. Vector Sequence Translation (Plural)
shape.move([[0, 0, 0], [0, 0.3, 0.3]])
```

If a sequence (`jot:vec3s`) is passed to `move()`, it maps over the sequence, producing a collection of translated shapes.

---

## 2. Parser Integration (`jot/src/parser.js`)

In `JotParser`, we modify the `_parseArrayOrRange()` method to check for the `do` keyword immediately following the first expression:

```javascript
_parseArrayOrRange() {
    this._consume('[');
    
    // 1. Detect range shorthands
    if (this._peek() === '..' || this._peek() === 'by' || this._peek() === 'count' || this._peek() === 'inc') {
        return this._parseRangeContent(null);
    }

    const firstExpr = this._parseExpression();
    
    // 2. Check for Map Comprehension
    if (this._peek() === 'do') {
        this._consume('do');
        const parameter = this._consume(); // Parameter name, e.g. '$' or 'y'
        this._consume('->');
        const expression = this._parseExpression(); // Projection expression, e.g. [0, $, $]
        this._consume(']');
        
        return {
            type: 'MAP_COMPREHENSION',
            iterable: firstExpr,
            parameter,
            expression
        };
    }

    // 3. Otherwise standard array or range parsing...
}
```

---

## 3. Compiler Integration (`jot/src/compiler.js`)

### A. Comprehension Node Evaluation
We add a handler for `MAP_COMPREHENSION` nodes in `_evaluateRecursive`:

```javascript
case 'MAP_COMPREHENSION': {
    const list = await this._evaluateRecursive(node.iterable, parameters, subject, ctx);
    if (!Array.isArray(list)) {
        throw new Error(`Compiler Error: Iterable in map comprehension must evaluate to an array. Got ${typeof list}`);
    }
    const results = [];
    for (const item of list) {
        // Bind the parameter in a clean child lexical scope
        const childParams = Object.create(parameters);
        childParams[node.parameter] = item;
        
        const val = await this._evaluateRecursive(node.expression, childParams, subject, ctx);
        results.push(val);
    }
    return results;
}
```

### B. `jot:vec3s` Consumer & Normalization Rules
We introduce the `jot:vec3s` type check, subtype promotion, and normalization:

1. **Subtype Promotion (`_isSubtype`)**:
   ```javascript
   if (a === 'jot:vec3' && e === 'jot:vec3s') return true;  // Singular to Plural Promotion
   if (a === 'jot:vec3s' && e === 'jot:vec3') return true;  // Plural to Singular Promotion
   ```

2. **Vector Type Checks**:
   ```javascript
   _isJotVec3s(v, a, c) {
       if (Array.isArray(v)) {
           if (this._isJotVec3(v, a, c)) return true; // Single vec promoted
           return v.every(item => this._isJotVec3(item, a, c));
       }
       if (v?.type === 'SYMBOL' && this._isSubtype(this._getTypeOfValue(v, c), 'jot:vec3s')) return true;
       return false;
   }
   ```

3. **Normalization (`_normalize`)**:
   ```javascript
   if (type === 'jot:vec3s') {
       // Single vector [x, y, z] promoted to list: [[x, y, z]]
       if (Array.isArray(val) && val.length === 3 && val.every(v => typeof v === 'number')) {
           return [val];
       }
       if (Array.isArray(val) && val.every(item => Array.isArray(item))) {
           return val;
       }
       if (!Array.isArray(val)) {
           return [val];
       }
   }
   ```

4. **Register Consumer**:
   Add `'jot:vec3s'` to `this.consumers`, mapping to a consumer similar to `JotShapesConsumer` that extracts individual `jot:vec3` elements or `jot:vec3s` arrays from the argument pool.

---

## 4. C++ Move Operation refactoring (`geo/ops/move_op.h`)

We update the `MoveOp` implementation in the C++ kernel to accept a coordinate sequence:

```cpp
template <typename P = JotVfsProtocol>
struct MoveOp : MoveOpBase<P> {
    static constexpr const char* path = "jot/move";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& offset) {
        // Note: The C++ binder deserializes the JSON array parameter 'offset' as a std::vector<double>.
        // When multiple vector offsets are passed from JOT (e.g. vector of vectors), 
        // the compiler maps over the list, invoking MoveOp on each vector.
        double x = offset.size() > 0 ? offset[0] : 0.0;
        double y = offset.size() > 1 ? offset[1] : 0.0;
        double z = offset.size() > 2 ? offset[2] : 0.0;
        MoveOpBase<P>::execute_multi(vfs, fulfilling, in, {Matrix::translate(x, y, z)});
    }

    static std::vector<std::string> argument_keys() { return {"$in", "offset"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/move"},
            {"description", "Translates the input shape by the given 3D translation offset vector."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to move."}}}}},
            {"arguments", json::array({
                {{"name", "offset"}, {"type", "jot:vec3"}, {"default", {0.0, 0.0, 0.0}}, {"description", "The offset vector."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The moved shape."}}}}}
        };
    }
};
```

*(Note: Since the compiler maps over sequences before dispatching to C++, the C++ operator signature itself is kept simple, operating on single vector `offset` inputs. The mapping behavior is handled transparently by the compiler's Universal Mapping Principle).*

---

## 5. Verification Plan

1. **Unit Tests (`jot/test/comprehension.test.js`)**:
   Verify parsing and compilation of map comprehensions:
   * Evaluate `[1..3 do x -> x * 2]` yields `[2, 4, 6]`.
   * Evaluate `[0..2 by 1 do $ -> [0, $, $]]` yields `[[0, 0, 0], [0, 1, 1], [0, 2, 2]]`.

2. **Integration Test (`integration/prompt_staircase.test.js`)**:
   Update the simulated JOT code to use the new comprehension syntax and verify end-to-end execution, solid union, and PNG snapshot generation:
   ```jot
   steps = Box(1.5, 0.4, 0.4).move([0 .. 2.7 by 0.3 do $ -> [0, $, $]])
   Fuse(steps) -> $out
   ```
