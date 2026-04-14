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
        const expr = this._parseExpression();
        console.log('[JotParser] AST:', JSON.stringify(expr, null, 2));
        if (this.pos < this.tokens.length) {
            throw new Error(`Unexpected token at end: ${this.tokens[this.pos]}`);
        }
        return expr;
    }

    _tokenize(text) {
        const tokens = [];
        // Ignore single-line comments
        const cleanText = text.replace(/\/\/.*$/gm, '');
        const regex = /\s*([a-zA-Z_][a-zA-Z0-9_]*|[0-9]+(?:\.[0-9]+)?|"[^"]*"|'[^']*'|\.\.\.|\.|\(|\)|\{|\}|:|\[|\]|,)\s*/g;
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

        if (token === '[') {
            return this._parseArray();
        }

        if (token === '{') {
            return this._parseObject();
        }

        if (token === '(') {
            this._consume('(');
            const expr = this._parseExpression();
            this._consume(')');
            return expr;
        }

        if (/^[0-9]/.test(token)) {
            return parseFloat(this._consume());
        }

        if (/^["']/.test(token)) {
            return this._consume().slice(1, -1);
        }

        if (/^[a-zA-Z_]/.test(token)) {
            const name = this._consume();
            if (this._peek() === '(') {
                const args = this._parseArguments();
                return { type: 'CALL', name, args };
            }
            return { type: 'SYMBOL', name };
        }

        throw new Error(`Unexpected token: ${token}`);
    }

    _parseArray() {
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
            args.push(this._parseExpression());
            if (this._peek() === ',') this._consume(',');
        }
        this._consume(')');
        return args;
    }

    _wrapInOp(name, subject, args) {
        return { type: 'METHOD', subject, name, args };
    }
}
