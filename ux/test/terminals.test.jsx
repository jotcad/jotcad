import { render, fireEvent, waitFor, screen } from '@solidjs/testing-library';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { JotNode } from '../src/components/JotNode';
import { blackboard, vfs } from '../src/lib/blackboard';

// Mock interactjs
vi.mock('interactjs', () => {
  const mock = vi.fn().mockReturnValue({
    draggable: vi.fn().mockReturnThis(),
    resizable: vi.fn().mockReturnThis(),
    on: vi.fn().mockReturnThis()
  });
  mock.modifiers = {
    restrictSize: vi.fn().mockReturnValue({})
  };
  return { default: mock };
});

// Mock Blackboard and VFS
vi.mock('../src/lib/blackboard', () => {
  return {
    vfs: {
      readData: vi.fn().mockResolvedValue({ type: 'mock-geometry' }),
      init: vi.fn().mockResolvedValue(true)
    },
    blackboard: {
      schemas: vi.fn().mockReturnValue({
        'jot/Box': {
          arguments: [{ name: 'size', type: 'number' }],
          outputs: { '$out': { type: 'shape' } }
        },
        'jot/pdf': {
          arguments: [
            { name: '$in', type: 'shape', affiliate: '$in' },
            { name: 'path', type: 'string' }
          ],
          outputs: {
            '$out': { type: 'shape' },
            'file': { type: 'file', mimeType: 'application/pdf' }
          }
        }
      }),
      setError: vi.fn(),
      dynamicOps: vi.fn().mockReturnValue({})
    },
    DEFAULT_CODE: 'Box(10)'
  };
});

// Mock ThreeUtils and Viewport to avoid WebGL issues in JSDOM
vi.mock('../src/lib/three_utils', () => ({
  packZFS: vi.fn().mockResolvedValue({ type: 'packed-geometry' })
}));

vi.mock('../src/components/viewport/Viewport', () => ({
  Viewport: () => <div data-testid="mock-viewport">Viewport</div>
}));

describe('JotNode Terminal Integration', () => {
  it('should display a download button when terminal files are discovered', async () => {
    const { unmount } = render(() => <JotNode id="test-node" />);
    
    // 1. Find the code editor (text area)
    const editor = screen.getByDisplayValue(/Box\(10\)/);
    
    // 2. Update code to produce a PDF terminal
    fireEvent.input(editor, { target: { value: 'Box(10).pdf("foo.pdf")' } });
    
    // 3. Trigger evaluation
    const evaluateBtn = screen.getByText('EVALUATE JOT');
    fireEvent.click(evaluateBtn);
    
    // 4. Wait for the download button to appear
    // The button label should be "foo.pdf" based on our routing logic
    await waitFor(() => {
      const downloadBtn = screen.queryByText('foo.pdf');
      expect(downloadBtn).toBeTruthy();
    }, { timeout: 2000 });
    
    unmount();
  });
});
