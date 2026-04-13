import { render } from '@solidjs/testing-library';
import { MeshMap } from '../src/components/MeshMap';
import { describe, it, expect } from 'vitest';

// Minimal mock for Blackboard to allow rendering
import { blackboard } from '../src/lib/blackboard';

describe('UX Smoke Test', () => {
  it('should import and render MeshMap without crashing', () => {
    // This should fail during import or execution if LucideIcon is missing
    const { unmount } = render(() => <MeshMap />);
    unmount();
  });
});
