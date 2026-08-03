import fs from 'node:fs';
import path from 'node:path';

// Helper to stringify complex objects as a cache key (matching normalizeId in AssetManager.js)
function normalizeId(id) {
  if (typeof id === 'string') return id;
  if (id && typeof id === 'object' && id.path) {
    const params = id.parameters || {};
    const sortedParams = Object.keys(params).sort().reduce((acc, key) => { acc[key] = params[key]; return acc; }, {});
    return id.path + '?' + JSON.stringify(sortedParams);
  }
  return JSON.stringify(id);
}

// Helper to format expressions recursively based on max line length (default 80 characters)
function formatExpression(expr, indentLevel = 0, maxLen = 80, baseSpaces = null) {
  const currentIndent = baseSpaces !== null ? ' '.repeat(baseSpaces) : '  '.repeat(indentLevel);
  
  // 1. Try horizontal layout first
  const horizontal = expr.replace(/\s+/g, ' ').trim();
  if (currentIndent.length + horizontal.length <= maxLen) {
    return horizontal;
  }

  // 2. If it exceeds maxLen, find top-level dots/arrows to split
  const splits = [];
  let inString = false;
  let stringChar = null;
  let inBracket = 0;
  let inParens = 0;

  for (let i = 0; i < expr.length; i++) {
    const char = expr[i];
    if ((char === '"' || char === "'" || char === '`') && expr[i - 1] !== '\\') {
      if (!inString) {
        inString = true;
        stringChar = char;
      } else if (char === stringChar) {
        inString = false;
        stringChar = null;
      }
    }
    if (inString) continue;

    if (char === '[') inBracket++;
    if (char === ']') inBracket--;
    if (char === '(') inParens++;
    if (char === ')') inParens--;

    if (inBracket === 0 && inParens === 0) {
      if (char === '.') {
        const nextChar = expr[i + 1];
        const isDigit = nextChar >= '0' && nextChar <= '9';
        if (!isDigit) {
          splits.push({ index: i, type: 'dot' });
        }
      } else if (char === '-' && expr[i + 1] === '>') {
        splits.push({ index: i, type: 'arrow' });
        i++; // skip '>'
      }
    }
  }

  if (splits.length === 0) {
    // No top-level splits, but exceeds maxLen. Check if it has brackets/parens we can format inside.
    const firstParen = expr.indexOf('(');
    const lastParen = expr.lastIndexOf(')');
    if (firstParen !== -1 && lastParen > firstParen) {
      const name = expr.slice(0, firstParen).trim();
      const argsStr = expr.slice(firstParen + 1, lastParen).trim();
      const trailing = expr.slice(lastParen + 1);
      
      const argOffset = (baseSpaces !== null ? baseSpaces : indentLevel * 2) + firstParen + 1;
      const formattedArgs = formatExpression(argsStr, indentLevel + 1, maxLen, argOffset + 2);
      return `${name}(${formattedArgs})${trailing}`;
    }
    return horizontal;
  }

  // Split into segments based on splits
  const segments = [];
  let lastIdx = 0;
  for (const s of splits) {
    segments.push(expr.slice(lastIdx, s.index).trim());
    lastIdx = s.index + (s.type === 'dot' ? 1 : 2);
  }
  segments.push(expr.slice(lastIdx).trim());

  // Format each segment recursively
  const formattedSegments = segments.map((seg, idx) => {
    const firstParen = seg.indexOf('(');
    const lastParen = seg.lastIndexOf(')');
    if (firstParen !== -1 && lastParen > firstParen) {
      const name = seg.slice(0, firstParen).trim();
      const argsStr = seg.slice(firstParen + 1, lastParen).trim();
      const trailing = seg.slice(lastParen + 1);
      
      let argOffset;
      if (idx === 0) {
        argOffset = (baseSpaces !== null ? baseSpaces : indentLevel * 2) + firstParen + 1;
      } else {
        const prevSplit = splits[idx - 1];
        const indentSpaces = baseSpaces !== null ? baseSpaces : (indentLevel + 1) * 2;
        const prefixLen = indentSpaces + (prevSplit.type === 'dot' ? 1 : 3) + name.length + 1;
        argOffset = prefixLen;
      }
      
      const formattedArgs = formatExpression(argsStr, indentLevel + 1, maxLen, argOffset + 2);
      return `${name}(${formattedArgs})${trailing}`;
    }
    return seg;
  });

  // Assemble vertically
  let result = formattedSegments[0];
  const nextIndent = baseSpaces !== null ? ' '.repeat(baseSpaces) : '  '.repeat(indentLevel + 1);
  for (let i = 1; i < formattedSegments.length; i++) {
    const splitType = splits[i - 1].type;
    if (splitType === 'dot') {
      result += `\n${nextIndent}.${formattedSegments[i]}`;
    } else {
      result += `\n${nextIndent}-> ${formattedSegments[i]}`;
    }
  }
  return result;
}

// Helper to prettify JOT scripts for rendering in the session viewer
function formatJotCode(code, maxLen = 60) {
  // First, split into individual statements (preserving strings/brackets)
  const statements = [];
  let currentStmt = '';
  let inString = false;
  let stringChar = null;
  let inBracket = 0;
  let inParens = 0;

  for (let i = 0; i < code.length; i++) {
    const char = code[i];
    
    if ((char === '"' || char === "'" || char === '`') && code[i - 1] !== '\\') {
      if (!inString) {
        inString = true;
        stringChar = char;
      } else if (char === stringChar) {
        inString = false;
        stringChar = null;
      }
    }

    currentStmt += char;

    if (inString) continue;

    if (char === '[') inBracket++;
    if (char === ']') inBracket--;
    if (char === '(') inParens++;
    if (char === ')') inParens--;

    if (char === ';') {
      if (inBracket === 0 && inParens === 0) {
        statements.push(currentStmt.trim());
        currentStmt = '';
        if (code[i + 1] === ' ') i++;
      }
    }
  }
  if (currentStmt.trim()) {
    statements.push(currentStmt.trim());
  }

  // Format each statement
  const formattedStatements = statements.map(stmt => {
    const hasSemicolon = stmt.endsWith(';');
    const cleanStmt = hasSemicolon ? stmt.slice(0, -1).trim() : stmt;
    
    const formatted = formatExpression(cleanStmt, 0, maxLen);
    return hasSemicolon ? formatted + ';' : formatted;
  });

  return formattedStatements.join('\n\n');
}

// Ported packZFS shape packaging logic from ux/src/lib/render/GeometryDecoder.js
async function packZFS(vfs, shape) {
  const assets = new Map();
  const walk = async (s) => {
    if (!s || typeof s !== 'object') return;
    if (s.geometry) {
      const id = normalizeId(s.geometry);
      if (!assets.has(id)) {
        try {
          const res = await vfs.readCID(id);
          if (res) {
            let text = '';
            if (res.data) {
              text = typeof res.data === 'string' ? res.data : new TextDecoder().decode(res.data);
            } else if (res.stream) {
              const chunks = [];
              for await (const chunk of res.stream) chunks.push(chunk);
              text = new TextDecoder().decode(Buffer.concat(chunks));
            }
            if (text) {
              assets.set(id, text);
            }
          }
        } catch (e) {
          console.warn('[packZFS] Failed to fetch VFS asset:', id, e);
        }
      }
    }
    if (s.components && Array.isArray(s.components)) {
      for (const sub of s.components) {
        await walk(sub);
      }
    }
  };
  await walk(shape);
  
  let zfs = '';
  const mainJson = JSON.stringify(shape);
  zfs += `=${mainJson.length} files/main.json\n${mainJson}\n`;
  for (const [id, text] of assets) {
    zfs += `=${text.length} assets/text/${id}\n${text}\n`;
  }
  return Buffer.from(zfs);
}

export class UGCSession {
  constructor(sessionDir, ugcEngine) {
    this.sessionDir = path.resolve(sessionDir);
    this.ugcEngine = ugcEngine;
  }

  /**
   * Initializes the base session directory
   */
  init() {
    if (!fs.existsSync(this.sessionDir)) {
      fs.mkdirSync(this.sessionDir, { recursive: true });
    }
  }

  /**
   * Computes the next sequence number by scanning the directory
   */
  _getNextSequence() {
    this.init();
    const files = fs.readdirSync(this.sessionDir);
    let maxSeq = 0;
    for (const f of files) {
      const match = f.match(/^(\d{3})_/);
      if (match) {
        const seq = parseInt(match[1], 10);
        if (seq > maxSeq) {
          maxSeq = seq;
        }
      }
    }
    const nextSeq = maxSeq + 1;
    return String(nextSeq).padStart(3, '0');
  }

  /**
   * Formats the current date and time as YYYYMMDD_HHMMSS
   */
  _getTimestamp() {
    const d = new Date();
    const pad = (num) => String(num).padStart(2, '0');
    const yyyy = d.getFullYear();
    const mm = pad(d.getMonth() + 1);
    const dd = pad(d.getDate());
    const hh = pad(d.getHours());
    const min = pad(d.getMinutes());
    const ss = pad(d.getSeconds());
    return `${yyyy}${mm}${dd}_${hh}${min}${ss}`;
  }

  /**
   * Compiles the script and exports all results into a new sequential snapshot subdirectory
   * @param {string} scriptContent 
   * @param {Object} cliInputs 
   * @param {Object} cliOutputs Mappings of port name -> target base filename (e.g. stl_file: 'output.stl')
   * @param {string} note A descriptive note summarizing the run operation
   * @returns {Promise<Object>} Run metadata
   */
  async createSnapshot(scriptContent, cliInputs = {}, cliOutputs = {}, note = 'run') {
    this.init();

    // 1. Setup sequence, timestamp, and snapshot folder
    const seq = this._getNextSequence();
    const timestamp = this._getTimestamp();
    const sanitizedNote = note.toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/(^_|_$)/g, '');
    const folderName = `${seq}_${timestamp}_${sanitizedNote || 'run'}`;
    const snapshotDir = path.join(this.sessionDir, folderName);

    fs.mkdirSync(snapshotDir, { recursive: true });
    console.log(`[UGCSession] Creating snapshot folder: ${snapshotDir}`);

    // 2. Save the source script copy
    const scriptPath = path.join(snapshotDir, 'script.jot');
    fs.writeFileSync(scriptPath, formatJotCode(scriptContent));

    // 3. Construct dynamic outputs schema matching required ports
    const schema = {
      outputs: Object.entries(cliOutputs).reduce((acc, [key, val]) => {
        const type = typeof val === 'object' ? val.type : 'jot:file';
        acc[key] = { type };
        return acc;
      }, {})
    };

    // 4. Run compilation & evaluation
    const startTime = Date.now();
    const results = await this.ugcEngine.evaluate(scriptContent, cliInputs, schema);
    const compileDuration = Date.now() - startTime;

    const exportedFiles = {};

    // 5. Drain format streams to files inside snapshot directory
    for (const { port, selector } of results) {
      const val = cliOutputs[port];
      if (!val) continue;

      const baseFilename = typeof val === 'object' ? val.path : val;
      const targetPath = path.join(snapshotDir, baseFilename);
      console.log(`[UGCSession] Draining port '${port}' (Selector: ${selector.path}) -> ${targetPath}`);

      const ext = path.extname(baseFilename).toLowerCase();
      if (ext === '.jot') {
        const streamResult = await this.ugcEngine.vfs.readSelector(selector);
        if (streamResult) {
          let rawData = streamResult.data;
          if (!rawData && streamResult.stream) {
            const chunks = [];
            for await (const chunk of streamResult.stream) chunks.push(chunk);
            rawData = Buffer.concat(chunks);
          }
          if (rawData) {
            const rawText = typeof rawData === 'string' ? rawData : new TextDecoder().decode(rawData);
            let shapeObj = JSON.parse(rawText);
            
            const packedBytes = await packZFS(this.ugcEngine.vfs, shapeObj);
            fs.writeFileSync(targetPath, packedBytes);
            exportedFiles[port] = {
              filename: baseFilename,
              size: packedBytes.length,
              cid: selector.cid || null
            };
            continue;
          }
        }
      }

      const streamResult = await this.ugcEngine.vfs.readSelector(selector);
      if (streamResult && streamResult.stream) {
        const chunks = [];
        for await (const chunk of streamResult.stream) {
          chunks.push(chunk);
        }
        const bytes = Buffer.concat(chunks);
        fs.writeFileSync(targetPath, bytes);
        exportedFiles[port] = {
          filename: baseFilename,
          size: bytes.length,
          cid: selector.cid || null
        };
      } else {
        throw new Error(`Failed to resolve output stream for port: ${port}`);
      }
    }

    // 6. Write run metadata file
    const runMetadata = {
      sequence: seq,
      timestamp,
      note,
      inputs: cliInputs,
      outputs: exportedFiles,
      compileDurationMs: compileDuration
    };

    fs.writeFileSync(
      path.join(snapshotDir, 'run.json'),
      JSON.stringify(runMetadata, null, 2)
    );

    console.log(`[UGCSession] Snapshot ${seq} successfully compiled in ${compileDuration}ms.`);
    return { snapshotDir, metadata: runMetadata };
  }

  /**
   * Lists all snapshot directories in chronological order
   */
  listSnapshots() {
    this.init();
    const items = fs.readdirSync(this.sessionDir);
    const snapshots = [];

    for (const item of items) {
      const match = item.match(/^(\d{3})_(\d{8}_\d{6})_(.+)$/);
      if (match) {
        const runJsonPath = path.join(this.sessionDir, item, 'run.json');
        let runData = null;
        if (fs.existsSync(runJsonPath)) {
          try {
            runData = JSON.parse(fs.readFileSync(runJsonPath, 'utf-8'));
          } catch (e) {}
        }
        snapshots.push({
          dirName: item,
          sequence: match[1],
          timestamp: match[2],
          note: match[3],
          run: runData
        });
      }
    }

    // Sort by sequence number
    return snapshots.sort((a, b) => a.sequence.localeCompare(b.sequence));
  }
}
