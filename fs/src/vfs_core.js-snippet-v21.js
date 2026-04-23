  async writeData(pathOrSelector, data) {
    let bytes;
    if (data instanceof Uint8Array) bytes = data;
    else if (typeof data === 'string') bytes = new TextEncoder().encode(data);
    else bytes = new TextEncoder().encode(JSON.stringify(data, null, 2));
    const stream = new WebReadableStream({
      start(controller) {
        controller.enqueue(bytes);
        controller.close();
      },
    });
    await this.write(pathOrSelector, stream);
  }

  async readData(pathOrSelector, parameters, context = {}) {
    // If first arg is an object with path, it's a selector.
    // If it's a string, we use the second arg as parameters.
    let s;
    if (pathOrSelector && typeof pathOrSelector === 'object' && pathOrSelector.path) {
        s = pathOrSelector;
        context = parameters || {};
    } else {
        s = { path: pathOrSelector, parameters: parameters || {} };
    }

    const result = await this._readResult(s, context);
    if (!result || !result.success) return null;
    const stream = await this.storage.get(result.cid);
    if (!stream) return null;
    const chunks = [];
    const reader = stream.getReader();
    try {
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
      }
    } finally {
      reader.releaseLock();
    }
    let len = chunks.reduce((acc, c) => acc + c.length, 0);
    const bytes = new Uint8Array(len);
    let offset = 0;
    for (const chunk of chunks) {
      bytes.set(chunk, offset);
      offset += chunk.length;
    }
    const text = new TextDecoder().decode(bytes);
    try {
      const trimmed = text.trim();
      if (trimmed.startsWith('{') || trimmed.startsWith('['))
        return JSON.parse(trimmed);
    } catch (e) {}
    return bytes;
  }

  async readText(pathOrSelector, parameters, context = {}) {
    const data = await this.readData(pathOrSelector, parameters, context);
    if (!data) return null;
    if (typeof data === 'string') return data;
    if (data instanceof Uint8Array) return new TextDecoder().decode(data);
    return JSON.stringify(data, null, 2);
  }
