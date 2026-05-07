export const normalizeId = (id) => {
  if (typeof id === 'string') return id;
  if (id && typeof id === 'object' && id.path) {
    const params = id.parameters || {};
    const sortedParams = Object.keys(params).sort().reduce((acc, key) => { acc[key] = params[key]; return acc; }, {});
    return id.path + '?' + JSON.stringify(sortedParams);
  }
  return JSON.stringify(id);
};

export class JOTAssets {
  constructor(vfs) { this.vfs = vfs; this.cache = new Map(); this.main = null; }
  async parse(text) {
    let offset = 0; if (text[0] === '\n') offset++;
    while (offset < text.length) {
      if (text[offset] !== '=') break;
      offset++;
      const nl = text.indexOf('\n', offset), h = text.substring(offset, nl);
      offset = nl + 1;
      const sp = h.indexOf(' '), len = parseInt(h.substring(0, sp), 10), name = h.substring(sp + 1);
      const content = text.substring(offset, offset + len);
      offset += len; if (text[offset] === '\n') offset++;
      if (name.startsWith('files/')) this.main = JSON.parse(content);
      else if (name.startsWith('assets/text/')) this.cache.set(name.substring(12), content);
    }
    return this.main;
  }
  async getText(id) {
    const cacheKey = normalizeId(id);
    if (this.cache.has(cacheKey)) return this.cache.get(cacheKey);
    if (!this.vfs) return null;
    try {
      const data = await this.vfs.readData(id);
      if (data) {
        const text = typeof data === 'string' ? data : new TextDecoder().decode(data);
        this.cache.set(cacheKey, text);
        return text;
      }
    } catch (e) { console.warn('[JOTAssets] Resolution failed:', id, e); }
    return null;
  }
}
