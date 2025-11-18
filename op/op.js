import { computeHash } from './hash.js';

export let graph;
export let ops; console.log('ops declared (initial):', ops);
export let nextId;

export const symbolPrefix = '\uE000 ';

export const isSymbol = (string) =>
  typeof string === 'string' && string.startsWith(symbolPrefix);
export const makeSymbol = (string) => symbolPrefix + string;

export const beginOps = (graphToUse = {}) => {
  graph = graphToUse;
  ops = []; console.log('ops assigned in beginOps:', ops);
  nextId = 0;
  return ops;
};

export const endOps = () => {
  const result = { graph, ops };
  graph = undefined;
  ops = undefined; console.log('ops assigned in endOps:', ops);
  nextId = undefined;
  return result;
};

export const emitOp = (op) => {
  console.log('ops in emitOp (before push):', ops);
  ops.push(op); console.log('ops in emitOp (after push):', ops);
  return op;
};

export class Op {
  static spec = {};
  static code = {};
  static specHandlers = [];

  constructor({
    name = null,
    effect = null,
    input = null,
    args = [],
    caller = null,
  }) {
    this.name = name;
    this.effect = effect;
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
    const tailOp = op;
    if (op) {
      while (op.input && !op.inputResolved) {
        op = op.input;
      }
      op.input = this;
      op.inputResolved = true;
    }
    return tailOp;
  }

  $calledBy(caller) {
    this.caller = caller;
    return this;
  }

  static registerOp({ name, spec, args, code }) {
    const [inputSpec, argSpecs, outputSpec] = spec;
    Op.checkSpec(inputSpec);
    Op.checkSpec(argSpecs);
    Op.checkSpec(outputSpec);

    const { [name]: method } = {
      [name]: function (...argList) {
        const input = this;
        const op = new Op({ name, input });
        op.args = Op.destructure(name, spec, op, argList);
        if (args) {
          op.args = args(input, ...op.args);
        }
        const setCaller = (arg, op) => {
          if (arg instanceof Op) {
            arg.caller = op;
          } else if (arg instanceof Array) {
            for (const element of arg) {
              setCaller(element, op);
            }
          } else if (arg instanceof Object) {
            for (const key of Object.keys(arg)) {
              setCaller(arg[key], op);
            }
          }
        };
        for (const arg of op.args) {
          setCaller(arg, op);
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

  static hasSpecHandler(spec) {
    for (const specHandler of Op.specHandlers) {
      if (specHandler(spec)) {
        return true;
      }
    }
    return false;
  }

  static checkSpec(spec) {
    if (spec === null) {
      return;
    }
    if (Op.hasSpecHandler(spec)) {
      return;
    }
    if (spec instanceof Array) {
      // It might be an array of specs, so check each one.
      for (const subSpec of spec) {
        Op.checkSpec(subSpec);
      }
    } else {
      // We don't have a handler for this spec, and it's not an array of specs.
      throw new Error(`No spec handler for type: ${spec}`);
    }
  }

  static resolveInput(op) {
    console.log(`QQ/resolveInput: op=${op.name}`);
    const { caller } = op;
    if (!caller) {
      // This is not an argument.
      // The input is already resolved.
      console.log(`QQ/resolveInput: no caller`);
      return;
    }
    const { input } = caller;
    if (!input) {
      // The caller has no input.
      // No input to resolve.
      console.log(`QQ/resolveInput: no input`);
      return;
    }
    // The caller has input to delegate;
    for (;;) {
      // Walk back to the head of this op chain.
      if (op.inputResolved) {
        console.log(`QQ/resolveInput: head`);
        break;
      }
      if (!op.input) {
        console.log(`QQ/resolveInput: linked`);
        // This is the head.
        // Set the input to the caller's input.
        op.input = input;
        op.inputResolved = true;
        break;
      }
      op = op.input;
    }
  }

  static resolveNode(value) {
    console.log(`QQ/resolveNode: value=${value}`);
    if (value instanceof Op) {
      console.log(`QQ/resolveNode: Op name=${value.name}`);
      if (value.node) {
        console.log(`QQ/resolveNode: value.node.output=${JSON.stringify(value.node.output)}`);
        return value.node.output;
      }
      const node = {
        name: value.name,
        input: value.input ? Op.resolveNode(value.input) : null,
        args: Op.resolveNode(value.args),
      };
      node.output = makeSymbol(computeHash(node));
      value.node = node;
      console.log(`QQ/resolveNode: node=${JSON.stringify(node)}`);
      return node.output;
    } else if (value instanceof Array) {
      console.log(`QQ/resolveNode: Array`);
      return value.map((op) => Op.resolveNode(op));
    } else if (value instanceof Object) {
      console.log(`QQ/resolveNode: Object`);
      const resolved = {};
      for (const key of Object.keys(value)) {
        resolved[key] = Op.resolveNode(value[key]);
      }
      return resolved;
    } else {
      console.log(`QQ/resolveNode: other`);
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

export const dumpGraphviz = async (path, ops) => {
  const nodes = [];
  for (const { node } of ops) {
    nodes.push(`"${node.output}" [label="${node.name}"];`);
  }
  const edges = [];
  for (const { node } of ops) {
    edges.push(`"${node.input}" -> "${node.output}";`);
    const walk = (arg) => {
      if (isSymbol(arg)) {
        edges.push(`"${arg}" -> "${node.output}";`);
      } else if (arg instanceof Array) {
        for (const elt of arg) {
          walk(elt);
        }
      } else if (arg instanceof Object) {
        for (const value of Object.values(arg)) {
          walk(value);
        }
      }
    };
    walk(node.args);
  }
  console.log(`
digraph jot {
  ${nodes.join('\n')}
  ${edges.join('\n')}
}
`);
};

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
  // await dumpGraphviz('/tmp/jotcat.gv', ops);
  for (const op of ops) {
    const { node } = op;
    if (graph[node.output] !== undefined) {
      // This node has already been computed.
      continue;
    }
    const promise = new Promise(async (resolve, reject) => {
      await isReady;
      const { args, input, name, output } = node;
      let evaluatedInput;
      console.log(`QQ/resolve: input for ${name}:`, input);
      if (isSymbol(input)) {
        evaluatedInput = await graph[input];
        console.log(`QQ/resolve: evaluatedInput for ${name} (from symbol):`, evaluatedInput);
      } else {
        evaluatedInput = input;
        console.log(`QQ/resolve: evaluatedInput for ${name} (direct):`, evaluatedInput);
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
        console.log(`QQ/resolve: Before Op.code[${name}] call. ops:`, ops);
	console.log(`QQ/op: op=${name} evaluatedInput=${JSON.stringify(evaluatedInput)}`);
        const result = Op.code[name](
          output,
          context,
          evaluatedInput,
          ...evaluatedArgs
        );
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
