export const registerUtilityOps = (vfs) => {
    vfs.registerProvider('jot/range', async (v, s) => {
        const { start = 0, stop = s.parameters.arg || 0, step = 1 } = s.parameters;
        const res = [];
        for (let i = start; i < stop; i += step) res.push(i);
        return new TextEncoder().encode(JSON.stringify(res));
      }, { schema: { 
            arguments: [{ name: 'start', type: 'number', default: 0 }, { name: 'stop', type: 'number' }, { name: 'step', type: 'number', default: 1 }],
            outputs: { $out: { type: 'jot:numbers' } }
         } }
    );

    vfs.registerProvider('jot/iota', async (v, s) => {
        const count = s.parameters.count || s.parameters.arg || 0;
        const res = Array.from({ length: count }, (_, i) => i);
        return new TextEncoder().encode(JSON.stringify(res));
      }, { schema: { 
            arguments: [{ name: 'count', type: 'number' }],
            outputs: { $out: { type: 'jot:numbers' } }
         } }
    );
};
