import { describe, it, expect } from 'vitest';
import { Merger } from '../src/lib/vfs/CloudSync';

describe('3-Way Merge Engine', () => {
    it('should perform a clean 3-way merge for non-conflicting text changes', () => {
        const baseOp = {
            script: "Line 1\nLine 2\nLine 3",
            schema: { version: 1, args: [] }
        };

        const localOp = {
            script: "Line 1 - Local Edit\nLine 2\nLine 3",
            schema: { version: 1, args: [], localOnly: true }
        };

        const remoteOp = {
            script: "Line 1\nLine 2\nLine 3 - Remote Edit",
            schema: { version: 2, args: [] }
        };

        const result = Merger.mergeOperator('test/op', localOp, baseOp, remoteOp);

        expect(result.hasConflicts).toBe(false);
        expect(result.script).toBe("Line 1 - Local Edit\nLine 2\nLine 3 - Remote Edit");
        
        // trimerge should combine JSON objects
        expect(result.schema.version).toBe(2);
        expect(result.schema.localOnly).toBe(true);
    });

    it('should detect and mark conflicts for overlapping text changes', () => {
        const baseOp = {
            script: "Original Line",
            schema: {}
        };

        const localOp = {
            script: "Local Version",
            schema: {}
        };

        const remoteOp = {
            script: "Remote Version",
            schema: {}
        };

        const result = Merger.mergeOperator('test/op', localOp, baseOp, remoteOp);

        expect(result.hasConflicts).toBe(true);
        expect(result.script).toContain('<<<<<<< LOCAL');
        expect(result.script).toContain('=======');
        expect(result.script).toContain('>>>>>>> REMOTE');
        expect(result.script).toContain('Local Version');
        expect(result.script).toContain('Remote Version');
    });

    it('should merge JSON layout state using trimerge', () => {
        const baseLayout = {
            "editor-1": { x: 100, y: 100, scale: 1 }
        };

        const localLayout = {
            "editor-1": { x: 200, y: 100, scale: 1 },
            "editor-2": { x: 0, y: 0, scale: 1 }
        };

        const remoteLayout = {
            "editor-1": { x: 100, y: 100, scale: 1.5 },
            "editor-3": { x: 500, y: 500, scale: 1 }
        };

        const result = Merger.mergeLayout(localLayout, baseLayout, remoteLayout);

        // editor-1 should have combined x (from local) and scale (from remote)
        expect(result["editor-1"].x).toBe(200);
        expect(result["editor-1"].scale).toBe(1.5);
        
        // New editors from both sides should be present
        expect(result["editor-2"]).toBeDefined();
        expect(result["editor-3"]).toBeDefined();
    });
});
