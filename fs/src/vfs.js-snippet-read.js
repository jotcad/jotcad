  async read(target, context = {}) {
    this._checkClosed();
    const packetContext = { ...context, stack: context.stack || [], expiresAt: context.expiresAt || Date.now() + 10000 };
    
    let s;
    if (typeof target === 'string') {
        if (/^[0-9a-f]{64}$/i.test(target)) {
            // It's a CID
            const result = await this._readResult(target, packetContext);
            if (result && result.success) return this.storage.get(result.cid);
            return null;
        } else {
            // It's a path string. 
            s = normalizeSelector(target);
            // Formal Rule: Use schema-defined primaryOutput if no port is specified.
            const schema = this.schemas.get(s.path);
            if (!s.output && !s.path.includes(':') && schema?.primaryOutput) {
                s.output = schema.primaryOutput;
            }
        }
    } else {
        s = normalizeSelector(target);
    }

    this.validateSelector(s);
    const addrKey = await getSelectorKey(s);
    
    // Always check local storage first. 
    if (await this.storage.has(addrKey)) {
        const info = await this._getStorageInfo(addrKey);
        if (context.followLinks === false) {
            return this.storage.get(addrKey);
        }
        if (info?.state === 'LINKED' && info?.target) {
            // Normal recursion path (handled in _readResult)
        } else {
            return this.storage.get(addrKey);
        }
    }

    // Computational selectors must trigger fulfillment even if not in storage
    const result = await this._readResult(s, packetContext);
    if (result && result.success) return this.storage.get(result.cid);
    
    // If followLinks is false and we failed, we might be doing a silent link check.
    if (context.followLinks === false) return null;

    // Fail loudly with descriptive error
    const errorMsg = result?.error || `Content not found for selector: ${s.path}`;
    throw new Error(`VFS Read Failure: ${errorMsg}`);
  }
