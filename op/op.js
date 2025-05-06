import { computeHash } from '@jotcad/sys';

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

  setInput(input) {
    this.node.input = input;
    return this;
  }

  getOutput() {
    return this.getId();
  }

  getNode() {
    return this.node;
  }
}

export const opCode = {};

Op.code = opCode;

export const registerOp = (name, signature, code) => {
  const resolve = (input, value) => {
    if (value instanceof Op) {
      return value.setInput(input).getId();
    }
    return value;
  };
  const { [name]: method } = {
    [name]: function (...args) {
      const input = this ? this.getId() : null;
      return emitOp(
        new Op({ name, input, args: args.map((arg) => resolve(input, arg)) })
      );
    },
  };
  Op.prototype[name] = method;
  Op.code[name] = code;
  return method;
};

Op.registerOp = registerOp;

export const resolve = async (context, graph, ops) => {
  let ready;
  const isReady = new Promise((resolve, reject) => {
    ready = resolve;
  });
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
      const evaluatedArgs = [];
      for (const value of node.args) {
        if (isSymbol(value)) {
          evaluatedArgs.push(await graph[value]);
        } else {
          evaluatedArgs.push(await value);
        }
      }
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
