import { JotParser } from './parser.js';
import { JotCompiler } from './compiler.js';

/**
 * Registers a VFS provider that evaluates .jot expressions.
 */
export function registerJotProvider(vfs, options = {}) {
  // DEPRECATED: jot/eval provider removed per user request.
}
