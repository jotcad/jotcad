import { computeHash } from './hash.js';

export let ops;
export let nextId;

export const symbolPrefix = '\uE000 ';

export const isSymbol = (string) =>
  typeof string === 'string' && string.startsWith(symbolPrefix);
export const makeSymbol = (string) => symbolPrefix + string;

export const beginOps = () => {
  ops = [];
  nextId = 0;
  return ops;
};

export const endOps = () => {
  const result = ops;
  ops = undefined;
  nextId = undefined;
  return result;
};

export const emitOp = (op) => {
  ops.push(op);
  return op;
};

export class Op {
  static specs = {};
  static code = {};
  static specHandlers = [];

  constructor(node) {
    this.node = node;
    this.node.output = null;
  }

  getId() {
    if (this.node.output === null) {
      this.node.output = makeSymbol(computeHash(this.node));
    }
    return this.node.output;
  }

  getInput() {
    return this.node.input;
  }

  setInput(input) {
    this.node.input = input;
    return this;
  }

  getOutput() {
    return this.getId();
  }

  getOutputType() {
    const [, , outputType] = Op.specs[this.node.name];
    return outputType;
  }

  getNode() {
    return this.node;
  }

  static registerOp(name, specs, code) {
    const { [name]: method } = {
      [name]: function (...args) {
        const input = this ? this.getId() : null;
        const destructuredArgs = Op.destructure(name, specs, input, args);
        return emitOp(new Op({ name, input, args: destructuredArgs }));
      },
    };
    Op.prototype[name] = method;
    Op.code[name] = code;
    Op.specs[name] = specs;
    return method;
  }

  static registerSpecHandler(code) {
    Op.specHandlers.push(code);
    return code;
  }

  static resolveOp(input, value) {
    // If we have Foo(Bar().Qux()) then Bar() will lack its input.
    // Set the input of Bar() in this case to be the input
    // of Foo so that it has the right context.
    //
    // However, note that Qux() should already have its input as
    // Bar() which should not be overridden.
    if (value instanceof Op) {
      if (value.getInput()) {
        return value;
      } else {
        return value.setInput(input).getId();
      }
    } else if (value instanceof Array) {
      return value.map((item) => Op.resolveOp(input, item));
    } else if (value instanceof Object) {
      const resolved = {};
      for (const key of Object.keys(value)) {
        resolved[key] = Op.resolveOp(input, value[key]);
      }
      return resolved;
    } else {
      return value;
    }
  }

  static destructure(name, specs, input, args) {
    const [inputSpec, argSpecs, outputSpec] = specs;
    const destructured = [];
    for (const spec of argSpecs) {
      const rest = [];
      let found = false;
      for (const specHandler of Op.specHandlers) {
        const handler = specHandler(spec);
        if (!handler) {
          continue;
        }
        const value = handler(spec, input, args, rest);
        const resolvedValue = Op.resolveOp(input, value);
        destructured.push(resolvedValue);
        args = rest;
        found = true;
        break;
      }
      if (!found) {
        throw new Error(
          `Cannot destructure: spec=${spec} args=${JSON.stringify(args)}`
        );
      }
      args = rest;
    }
    return destructured;
  }
}

export const specEquals = (a, b) => {
  if (a === b) {
    return true;
  } else if (
    a instanceof Array &&
    b instanceof Array &&
    a.length === b.length
  ) {
    for (let nth = 0; nth < a.length; nth++) {
      if (a[nth] !== b[nth]) {
        return false;
      }
    }
    return true;
  } else {
    return false;
  }
};

export const predicateValueHandler = (name, predicate) => (spec) =>
  spec === name &&
  ((spec, input, args, rest) => {
    let result;
    while (args.length >= 1) {
      const arg = args.shift();
      if (
        predicate(arg) ||
        (arg instanceof Op && specEquals(arg.getOutputType(), spec))
      ) {
        result = arg;
        break;
      }
      rest.push(arg);
    }
    rest.push(...args);
    return result;
  });

export const resolve = async (context, graph, ops) => {
  let ready;
  const isReady = new Promise((resolve, reject) => {
    ready = resolve;
  });
  if (ops === undefined) {
    throw Error('resolve: ops=undefined');
  }
  for (const op of ops) {
    const node = op.getNode();
    if (graph[op.getOutput()] !== undefined) {
      // This node has already been computed.
      continue;
    }
    const dd = JSON.stringify(op);
    const promise = new Promise(async (resolve, reject) => {
      await isReady;
      const name = node.name;
      const evaluatedInput = await graph[node.input];
      const evaluateArgs = async (args) => {
        const evaluatedArgs = [];
        for (const value of args) {
          if (isSymbol(value)) {
            evaluatedArgs.push(await graph[value]);
          } else if (value instanceof Op) {
            evaluatedArgs.push(await graph[value.getOutput()]);
          } else if (value instanceof Array) {
            evaluatedArgs.push(await evaluateArgs(value));
          } else {
            evaluatedArgs.push(await value);
          }
        }
        return evaluatedArgs;
      };
      const evaluatedArgs = await evaluateArgs(node.args);
      try {
        const result = Op.code[name](context, evaluatedInput, ...evaluatedArgs);
        resolve(result);
      } catch (e) {
        throw e;
      }
    });
    promise.name = dd;
    graph[op.getOutput()] = promise;
  }
  ready();
  for (const [key, value] of Object.entries(graph)) {
    graph[key] = await value;
  }
};

export const run = async (context, code) => {
  const graph = {};
  beginOps();
  await code();
  await resolve(context, graph, ops);
  endOps();
  return graph;
};
