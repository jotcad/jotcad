// compiler.js
import * as escodegen from 'escodegen';
import { findNodeAt, simple } from 'acorn-walk';
import { parse } from 'acorn';

/**
 * Validates a CallExpression node against the provided whitelist.
 * @param {object} node The CallExpression AST node.
 * @param {object} whitelist The whitelist of allowed functions and methods.
 */
const validateCall = (node, whitelist) => {
  const callee = node.callee;
  if (!callee || typeof callee.type === 'undefined') {
    throw new Error('Invalid or malformed CallExpression: missing callee.');
  }

  if (callee.type === 'Identifier') {
    if (!whitelist.functions.includes(callee.name)) {
      throw new Error(`Invalid function call: ${callee.name}`);
    }
  } else if (callee.type === 'MemberExpression') {
    const methodName = callee.property.name;
    if (!whitelist.methods.includes(methodName)) {
      throw new Error(`Invalid method call: ${methodName}`);
    }
  }
};

/**
 * A compiler that validates against a provided whitelist of AST node types and global variables.
 * @param {string} sourceCode The input source code string.
 * @param {object} whitelist The whitelist object.
 * @returns {string} The compiled and validated JavaScript code string.
 */
export const compile = (sourceCode, whitelist) => {
  let ast;
  try {
    ast = parse(sourceCode, { ecmaVersion: 2020 });
  } catch (e) {
    throw new Error(`Parsing failed: ${e.message}`);
  }

  const declaredVariables = new Set();
  const allowedGlobals = new Set(whitelist.globals);

  // Pass 1: Collect all 'var' declarations
  simple(ast, {
    VariableDeclaration(node) {
      // Allow 'var', 'let', and 'const'. The sandbox prevents access to global Node.js objects.
      node.declarations.forEach((decl) => {
        if (decl.id && decl.id.type === 'Identifier') {
          declaredVariables.add(decl.id.name);
        }
      });
    },
  });

  // Pass 2: Validate all nodes against the whitelist
  simple(ast, {
    '*': (node) => {
      if (!whitelist.nodes.includes(node.type)) {
        throw new Error(`Invalid AST node type: ${node.type}`);
      }
    },
    Identifier: (node) => {
      const parent = findNodeAt(ast, node.start, node.end);
      const grandParent = parent ? parent.parent : null;
      if (
        grandParent &&
        grandParent.type !== 'MemberExpression' &&
        grandParent.type !== 'CallExpression' &&
        !declaredVariables.has(node.name) &&
        !allowedGlobals.has(node.name)
      ) {
        throw new Error(
          `Invalid undeclared variable or global: '${node.name}' is not allowed.`
        );
      }
    },
    CallExpression: (node) => {
      validateCall(node, whitelist);
    },
    BinaryExpression: (node) => {
      if (!whitelist.operators.includes(node.operator)) {
        throw new Error(`Invalid operator: ${node.operator}`);
      }
    },
  });

  return escodegen.generate(ast);
};
