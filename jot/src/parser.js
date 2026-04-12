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
        this.tokens = this._tokenize(text);
        this.pos = 0;
        const expr = this._parseExpression();
        if (this.pos < this.tokens.length) {
            throw new Error(`Unexpected token at end: ${this.tokens[this.pos]}`);
        }
        return expr;
    }

    _tokenize(text) {
        const tokens = [];
        const regex = /\s*([a-zA-Z_][a-zA-Z0-9_]*|[0-9]+(?:\.[0-9]+)?|"[^"]*"|'[^']*'|\.\.\.|\.|\(|\)|\{|\}|:|\[|\]|,)\s*/g;
        let match;
        while ((match = regex.exec(text)) !== null) {
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
                return this._createSelector(name, args);
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

    _createSelector(name, args) {
        const constructors = {
            box: 'shape/box',
            arc: 'shape/arc',
            tri: 'shape/triangle',
            orb: 'shape/orb',
            pt: 'shape/point',
            origin: 'shape/origin'
        };

        const path = constructors[name] || `op/${name}`;
        let parameters = {};
        
        if (args.length === 1 && typeof args[0] === 'object' && args[0].type !== 'SYMBOL' && !Array.isArray(args[0])) {
            parameters = args[0];
        } else {
            if (name === 'box') {
                if (args.length >= 1) parameters.width = args[0];
                if (args.length >= 2) parameters.height = args[1];
                if (args.length >= 3) parameters.depth = args[2];
            } else if (name === 'pt') {
                parameters.x = args[0] || 0;
                parameters.y = args[1] || 0;
                parameters.z = args[2] || 0;
            } else {
                parameters.args = args;
            }
        }

        return normalizeSelector(path, parameters);
    }

    _wrapInOp(opName, source, args) {
        const ops = {
            rx: 'op/rotateX',
            ry: 'op/rotateY',
            rz: 'op/rotateZ',
            move: 'op/move',
            at: 'op/move',
            size: 'op/size',
            sz: 'op/size',
            extrude: 'op/extrude',
            offset: 'op/offset',
            outline: 'op/outline',
            points: 'op/points',
            spec: 'op/spec'
        };

        const path = ops[opName] || `op/${opName}`;
        let parameters = { source };
        
        if (opName === 'spec') {
            parameters.spec = args[0];
        } else if (opName === 'rx' || opName === 'rotateX') parameters.turns = args[0];
        else if (opName === 'ry' || opName === 'rotateY') parameters.turns = args[0];
        else if (opName === 'rz' || opName === 'rotateZ') parameters.turns = args[0];
        else if (opName === 'offset') parameters.radius = args[0];
        else if (opName === 'extrude') parameters.height = args[0];
        else if (args.length > 0) parameters.args = args;

        return normalizeSelector(path, parameters);
    }
}
