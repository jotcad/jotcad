import { normalizeSelector } from '../../fs/src/vfs_core.js';

/**
 * JotCAD Secure Parser
 *
 * Transforms JS-like geometry expressions into deterministic VFS selectors.
 */
export class JotParser {
  constructor() {
    this.tokens = [];
    this.pos = 0;
  }

  parse(text) {
    if (!text || !text.trim()) return null;
    console.log('[JotParser] Tokenizing:', text);
    this.tokens = this._tokenize(text);
    console.log('[JotParser] Tokens:', this.tokens);
    this.pos = 0;
    const results = [];
    while (this.pos < this.tokens.length) {
      // Skip semicolons between expressions
      if (this._peek() === ';') {
        this._consume(';');
        continue;
      }
      const expr = this._parseExpression();
      console.log('[JotParser] AST Chunk:', JSON.stringify(expr, null, 2));
      results.push(expr);
    }
    return results.length === 1 ? results[0] : results;
  }

  _tokenize(text) {
    const tokens = [];
    const cleanText = text.replace(/\/\/.*$/gm, '');
    const regex =
      /\s*([a-zA-Z_][a-zA-Z0-9_/]*|[0-9]+(?:\.[0-9]+)?|"[^"]*"|'[^']*'|\.\.\.|\.\.|\.|\(|\)|\{|\}|=|:|\[|\]|,|;)\s*/g;
    let match;
    while ((match = regex.exec(cleanText)) !== null) {
      tokens.push(match[1]);
    }
    return tokens;
  }

  _peek() {
    return this.tokens[this.pos];
  }

  _consume(expected) {
    if (this.pos >= this.tokens.length) {
      throw new Error(`Expected ${expected || 'token'}, got end of input`);
    }
    const token = this.tokens[this.pos++];
    if (expected && token !== expected) {
      throw new Error(`Expected ${expected}, got ${token}`);
    }
    return token;
  }

  _parseExpression() {
    let expr = this._parsePrimary();

    while (this._peek() === '.') {
      this._consume('.');
      const name = this._consume();
      if (this._peek() !== '(') {
        throw new Error(`Expected ( after method name, got ${this._peek()}`);
      }
      const args = this._parseArguments();
      expr = this._wrapInOp(name, expr, args);
    }

    return expr;
  }

  _parsePrimary() {
    const token = this._peek();
    if (!token) throw new Error('Unexpected end of input');

    if (token === '(') {
      this._consume('(');
      const expr = this._parseExpression();
      this._consume(')');
      return expr;
    }

    if (token === '[') {
       return this._parseArrayOrRange();
    }

    if (token === '{') return this._parseObject();

    if (/^[0-9]/.test(token)) return parseFloat(this._consume());
    if (/^["']/.test(token)) return this._consume().slice(1, -1);

    if (/^[a-zA-Z_]/.test(token)) {
      const name = this._consume();
      if (name === 'true') return true;
      if (name === 'false') return false;
      if (name === 'null') return null;
      if (this._peek() === '(') {
        const args = this._parseArguments();
        return { type: 'CALL', name, args };
      }
      return { type: 'SYMBOL', name };
    }

    throw new Error(`Unexpected token: ${token}`);
  }

  _parseArrayOrRange() {
    this._consume('[');
    const next = this._peek();
    
    // Shorthand range [..10], [by 0.1], [count 5], [inc]
    if (next === '..' || next === 'by' || next === 'count' || next === 'inc') {
        return this._parseRangeContent(null);
    }

    // Could be an array [1, 2] or a range [0..10]
    const firstExpr = this._parseExpression();
    const afterFirst = this._peek();

    if (afterFirst === ',' || afterFirst === ']') {
        const elements = [firstExpr];
        while (this._peek() === ',') {
            this._consume(',');
            elements.push(this._parseExpression());
        }
        this._consume(']');
        return elements;
    }

    // It's a range starting with an expression [0..10]
    return this._parseRangeContent(firstExpr);
  }

  _parseRangeContent(startExpr) {
    let start = startExpr;
    let end = null;
    let step = null;
    let count = null;
    let inclusiveEnd = false;

    if (this._peek() === '..') {
      this._consume('..');
      end = this._parseExpression();
    }

    if (this._peek() === 'by') {
      this._consume('by');
      step = this._parseExpression();
    } else if (this._peek() === 'count') {
      this._consume('count');
      count = this._parseExpression();
    }

    if (this._peek() === 'inc') {
      this._consume('inc');
      inclusiveEnd = true;
    }

    this._consume(']');

    return {
      type: 'RANGE',
      start,
      end,
      step,
      count,
      inclusiveEnd
    };
  }

  _parseArray() {
    // Legacy - replaced by _parseGroupOrRange but kept for internal logic if needed
    this._consume('[');
    const elements = [];
    while (this._peek() && this._peek() !== ']') {
      elements.push(this._parseExpression());
      if (this._peek() === ',') this._consume(',');
    }
    this._consume(']');
    return elements;
  }

  _parseObject() {
    this._consume('{');
    const obj = {};
    while (this._peek() && this._peek() !== '}') {
      const key = this._consume();
      this._consume(':');
      const val = this._parseExpression();
      obj[key] = val;
      if (this._peek() === ',') this._consume(',');
    }
    this._consume('}');
    return obj;
  }

  _parseArguments() {
    this._consume('(');
    const args = [];
    while (this._peek() && this._peek() !== ')') {
      let typeHint = null;
      let nameHint = null;

      // 1. Check for Type Hint (Greedy identifier:identifier:... sequence)
      let p = this.pos;
      let hintParts = [];
      while (p + 1 < this.tokens.length && /^[a-zA-Z_]/.test(this.tokens[p]) && this.tokens[p+1] === ':') {
          if (p + 2 < this.tokens.length && this.tokens[p+2] === '=') break;
          hintParts.push(this.tokens[p]);
          p += 2;
      }
      if (hintParts.length > 0) {
          typeHint = hintParts.join(':');
          this.pos = p;
      }

      // 2. Check for Named Argument (identifier =)
      if (this.pos + 1 < this.tokens.length && /^[a-zA-Z_]/.test(this.tokens[this.pos]) && this.tokens[this.pos+1] === '=') {
        nameHint = this._consume();
        this._consume('=');
      }

      const value = this._parseExpression();
      
      if (typeHint || nameHint) {
        args.push({ type: 'ANNOTATED_ARG', typeHint, nameHint, value });
      } else {
        args.push(value);
      }

      if (this._peek() === ',') this._consume(',');
    }
    this._consume(')');
    return args;
  }

  _wrapInOp(name, subject, args) {
    return { type: 'METHOD', subject, name, args };
  }
}
