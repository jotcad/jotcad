import { log } from '../log.js';
import { 
  normalizeSelector, 
  Selector,
  isSelector,
  isString,
  getSelectorKey
} from '../cid.js';

export async function _readResult(vfs, target, context = {}) {
    const { 
        stack = [], 
        resolutionStack = [],
        expiresAt = Date.now() + 10000, 
        followLinks = true, 
        depth = 0 
    } = context;
    
    let s = null;
    let targetCID = null;

    if (isString(target)) {
        targetCID = target.toString();
    } else {
        s = normalizeSelector(target);
        vfs.validateSelector(s);
        targetCID = await getSelectorKey(s);
    }

    if (depth > 20) throw new Error(`Maximum recursion depth exceeded for ${targetCID}`);
    if (Date.now() > expiresAt) return { success: false, error: 'Expired' };

    if (resolutionStack.includes(targetCID)) {
         throw new Error(`Circular link detected for ${targetCID}`);
    }
    
    if (vfs.activeWait.has(targetCID)) return await vfs.activeWait.get(targetCID);

    const workPromise = (async () => {
      try {
        if (await vfs.storage.has(targetCID)) {
          const info = await vfs._getStorageInfo(targetCID);
          if (followLinks && info?.encoding === 'link') {
            const stream = await vfs.storage.get(targetCID);
            if (!stream) throw new Error(`VFS Link Failure: Missing data for link ${targetCID}`);
            
            const reader = stream.getReader();
            const chunks = [];
            while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                chunks.push(value);
            }
            const len = chunks.reduce((acc, c) => acc + c.length, 0);
            const bytes = new Uint8Array(len);
            let offset = 0;
            for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
            
            const targetObj = JSON.parse(new TextDecoder().decode(bytes));
            const s_target = Selector.fromObject(targetObj);

            return await _readResult(vfs, s_target, { 
                ...context, 
                depth: depth + 1, 
                resolutionStack: [...resolutionStack, targetCID] 
            });
          } else {
            return { success: true, cid: targetCID, metadata: info || {} };
          }
        }

        const isBackflow = stack.includes(vfs.id);
        const nextStack = [...stack, vfs.id]; 

        let resultData = null, meshInfo = {};
        if (s) {
            const provider = vfs._findProvider(s.path);
            if (provider) {
                const res = await provider(vfs, s, context);
                if (res === null) {
                    resultData = null;
                } else if (isSelector(res)) {
                    resultData = res;
                } else {
                    // Protocol Strictness: Providers MUST return { stream, metadata }
                    if (!res || !res.stream || !res.metadata) {
                        throw new Error(`VFS Protocol Violation: Provider for '${s.path}' must return { stream, metadata }. Got: ${typeof res}`);
                    }
                    resultData = res.stream;
                    meshInfo = res.metadata;
                }
            }
        }

        if (resultData === null && vfs.mesh && !isBackflow) {
          const meshResult = s 
            ? await vfs.mesh.readSelector(s, { ...context, stack: nextStack, resolutionStack })
            : await vfs.mesh.readCID(targetCID, { ...context, stack: nextStack, resolutionStack });
            
          if (meshResult) {
              // Protocol Strictness: MeshLink.read MUST return { stream, metadata }
              if (!meshResult.stream || !meshResult.metadata) {
                  throw new Error(`VFS Protocol Violation: MeshLink.read must return { stream, metadata }.`);
              }
              resultData = meshResult.stream;
              meshInfo = meshResult.metadata;
          }
        }
        
        if (resultData === null && isBackflow) {
            return { success: false, error: 'Backflow' };
        }

        if (resultData !== null && resultData !== undefined) {
          if (s && isSelector(resultData)) {
              await vfs.link(s, resultData);
              return await _readResult(vfs, resultData, context);
          }

          if (s) {
              const { cid } = await vfs.write(s, resultData, { ...meshInfo, ...context });
              const meta = await vfs._getStorageInfo(cid);
              return { success: true, cid, metadata: meta || {} };
          } else {
              const { cid } = await vfs.write(targetCID, resultData, { ...meshInfo, ...context, state: 'AVAILABLE' });
              const meta = await vfs._getStorageInfo(cid);
              return { success: true, cid, metadata: meta || {} };
          }
        }
        return { success: true, cid: null };
      } catch (err) {
        log(`[VFS ${vfs.id}] _readResult failed for ${targetCID}: ${err.message}`);
        return { success: false, error: err.message };
      } finally { vfs.activeWait.delete(targetCID); }
    })();

    vfs.activeWait.set(targetCID, workPromise);
    return await workPromise;
}
