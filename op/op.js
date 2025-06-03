import { computeHash } from './hash.js';

export let graph;
export let ops;
export let nextId;

export const symbolPrefix = '\uE000 ';

export const isSymbol = (string) =>
  typeof string === 'string' && string.startsWith(symbolPrefix);
export const makeSymbol = (string) => symbolPrefix + string;

export const beginOps = (graphToUse = {}) => {
  graph = graphToUse;
  ops = [];
  nextId = 0;
  return ops;
};

export const endOps = () => {
  const result = { graph, ops };
  graph = undefined;
  ops = undefined;
  nextId = undefined;
  return result;
};

export const emitOp = (op) => {
  ops.push(op);
  return op;
};

export class Op {
  static spec = {};
  static code = {};
  static specHandlers = [];

  constructor({ name = null, input = null, args = [], caller = null }) {
    this.name = name;
    this.input = input;
    this.args = args;
    this.caller = null;
    this.node = null;
  }

  getId() {
    if (this.node.output === null) {
      this.node.output = makeSymbol(computeHash(this.node));
    }
    return this.node.output;
  }

  getOutput() {
    return this.getId();
  }

  getOutputType() {
    const [, , outputType] = Op.spec[this.name];
    return outputType;
  }

  $chain(op) {
    if (op) {
      op.input = this;
      return op;
    } else {
      return this;
    }
  }

  static registerOp({ name, spec, args, code }) {
    const { [name]: method } = {
      [name]: function (...argList) {
        const input = this;
        const op = new Op({ name, input });
        op.args = Op.destructure(name, spec, op, argList);
        if (args) {
          op.args = args(...op.args);
        }
        for (const arg of op.args) {
          if (arg instanceof Op) {
            arg.caller = op;
          }
        }
        emitOp(op);
        return op;
      },
    };
    Op.prototype[name] = method;
    Op.code[name] = code;
    Op.spec[name] = spec;
    return method;
  }

  static registerSpecHandler(code) {
    Op.specHandlers.push(code);
    return code;
  }

  static resolveInput(op) {
    const { caller } = op;
    if (!caller) {
      // This is not an argument.
      // The input is already resolved.
      return;
    }
    const { input } = caller;
    if (!input) {
      // The caller has no input.
      // No input to resolve.
      return;
    }
    // The caller has input to delegate;
    for (;;) {
      // Walk back to the head of this op chain.
      if (!op.input) {
        // This is the head.
        // Set the input to the caller's input.
        op.input = input;
        break;
      }
      op = op.input;
    }
  }

  static resolveNode(value) {
    if (value instanceof Op) {
      if (value.node) {
        return value.node.output;
      }
      const node = {
        name: value.name,
        input: value.input ? Op.resolveNode(value.input) : null,
        args: Op.resolveNode(value.args),
      };
      node.output = makeSymbol(computeHash(node));
      value.node = node;
      return node.output;
    } else if (value instanceof Array) {
      return value.map((op) => Op.resolveNode(op));
    } else if (value instanceof Object) {
      const resolved = {};
      for (const key of Object.keys(value)) {
        resolved[key] = Op.resolveNode(value[key]);
      }
      return resolved;
    } else {
      return value;
    }
  }

  static destructure(name, spec, caller, args) {
    const [inputSpec, argSpecs, outputSpec] = spec;
    const destructured = [];
    for (const spec of argSpecs) {
      const rest = [];
      let found = false;
      for (const specHandler of Op.specHandlers) {
        const handler = specHandler(spec);
        if (!handler) {
          continue;
        }
        const value = handler(spec, caller, args, rest);
        // const resolvedValue = Op.resolveOp(input, value);
        destructured.push(value);
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
  ((spec, caller, args, rest) => {
    let result;
    while (args.length >= 1) {
      const arg = args.shift();
      if (predicate(arg)) {
        result = arg;
        break;
      } else if (arg instanceof Op && specEquals(arg.getOutputType(), spec)) {
        result = arg;
        break;
      }
      rest.push(arg);
    }
    rest.push(...args);
    return result;
  });

export const resolve = async (context, ops, graph) => {
  let assertIsReady;
  const isReady = new Promise((resolve, reject) => {
    assertIsReady = resolve;
  });
  if (ops === undefined) {
    throw Error('resolve: ops=undefined');
  }
  // Link up argument chains to their caller's input.
  for (const op of ops) {
    Op.resolveInput(op);
  }
  const nodes = [];
  for (const op of ops) {
    nodes.push(Op.resolveNode(op));
  }
  for (const op of ops) {
    const { node } = op;
    if (graph[node.output] !== undefined) {
      // This node has already been computed.
      continue;
    }
    const promise = new Promise(async (resolve, reject) => {
      await isReady;
      const { input, name, args } = node;
      let evaluatedInput;
      if (isSymbol(input)) {
        evaluatedInput = await graph[input];
      }
      const evaluateArg = async (arg) => {
        if (isSymbol(arg)) {
          const value = await graph[arg];
          if (value === undefined) {
            throw error('null graph value');
          }
          return value;
        } else if (arg instanceof Array) {
          const evaluated = [];
          for (const element of arg) {
            evaluated.push(await evaluateArg(element));
          }
          return evaluated;
        } else if (arg instanceof Object) {
          const evaluated = {};
          for (const key of Object.keys(arg)) {
            evaluated[key] = await evaluateArg(arg[key]);
          }
          return evaluated;
        } else {
          return arg;
        }
      };
      const evaluatedArgs = await evaluateArg(node.args);
      try {
        const result = Op.code[name](context, evaluatedInput, ...evaluatedArgs);
        resolve(result);
      } catch (e) {
        throw e;
      }
    });
    graph[node.output] = promise;
  }
  return { assertIsReady, graph };
};

const execute = async ({ assertIsReady, graph }) => {
  assertIsReady();
  for (const [key, value] of Object.entries(graph)) {
    graph[key] = await value;
  }
};

export const run = async (context, code, graphToUse) => {
  beginOps(graphToUse);
  await code();
  const { assertIsReady, graph: resolvedGraph } = await resolve(
    context,
    ops,
    graph
  );
  endOps();
  assertIsReady();
  await execute({ assertIsReady, graph: resolvedGraph });
  return resolvedGraph;
};
