# How to Add New Ops

This document explains how to add new operations (ops) to Jot.

There are two types of ops:

- **Constructors**: These create new shapes from scratch. Examples include
  `Box`, `Link`, and `Loop`.
- **Operations**: These modify existing shapes. Examples include `cut`, `link`,
  and `loop`.

## Defining a New Op

To define a new op, you need to:

1. Create a new file in the `ops/` directory (e.g., `ops/myOp.js`).
2. In the new file, import `registerOp` from `./op.js`.
3. Call `registerOp` with a configuration object.

The configuration object has the following properties:

- `name`: The name of the op.
- `spec`: An array that defines the arguments of the op.
- `code`: A function that implements the logic of the op.

### The `spec` Array

The `spec` array defines the signature of your op. The last element of the array
is the return type.

- For a **constructor**, the first element of the spec should be `null`.
- For an **operation**, the first element of the spec should be the type of the
  input shape (e.g., `'shape'`).

The elements in between are the arguments to your op. These can be primitive
types like `'string'`, `'number'`, `'boolean'`, or complex types like
`['shapes']` (an array of shapes) or `['options', { optionName: 'type' }]`.

### The `code` Function

The `code` function implements the logic of your op. It receives the following
arguments:

- `id`: The ID of the op.
- `context`: The session context, which contains `assets`.
- `input`: The input shape (for operations). For constructors, this will be
  `null`.
- ... The rest of the arguments defined in your `spec`.

### Adding the Op to `ops/main.js`

After creating your op, you need to export it from `ops/main.js`.

```javascript
// ops/main.js
export { myOp } from './myOp.js';
```

### Constructor Example: `Link`

Here is the definition of the `Link` constructor op from `ops/link.js`:

```javascript
export const Link = registerOp({
  name: 'Link',
  spec: [null, ['shapes'], ['options', { reverse: 'boolean' }], 'shape'],
  code: (id, context, input, shapes, { reverse = false } = {}) =>
    geometryLink(context.assets, shapes, false, reverse),
});
```

- The `spec` starts with `null`, indicating it's a constructor.
- It takes an array of shapes and an options object with a `reverse` boolean.
- The `code` function calls `geometryLink` with `close=false`.

### Operation Example: `link`

Here is the definition of the `link` operation op from `ops/link.js`:

```javascript
export const link = registerOp({
  name: 'link',
  spec: ['shape', ['shapes'], ['options', { reverse: 'boolean' }], 'shape'],
  code: (id, context, input, shapes, { reverse = false } = {}) =>
    geometryLink(context.assets, [input, ...shapes], false, reverse),
});
```

- The `spec` starts with `'shape'`, indicating it's an operation on a shape.
- It takes an input shape, an array of shapes, and an options object.
- The `code` function combines the input shape with the other shapes and calls
  `geometryLink` with `close=false`.

## Relationship with `geometry/`

Operations defined in `ops/` typically serve as high-level, user-facing
commands. They often act as lightweight wrappers that orchestrate calls to the
more complex, performance-sensitive geometric operations implemented in C++
within the `geometry/` module. This separation of concerns allows for efficient
core geometric processing while providing a flexible and user-friendly
JavaScript interface.

## Server Integration

To make a new operation available to the server and its web interface, you need
to export it from `ops/main.js` within the `constructors` or `operators`
objects, depending on whether it's a constructor or an operation. This ensures
the operation is properly exposed and accessible within the JotCAD ecosystem.
