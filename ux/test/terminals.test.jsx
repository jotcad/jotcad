import { render, fireEvent, waitFor, screen } from '@solidjs/testing-library';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { JotNode } from '../src/components/editor/JotNode';
import { blackboard, vfs } from '../src/lib/blackboard';

// Mock ResizeObserver for JSDOM
global.ResizeObserver = vi.fn().mockImplementation(() => ({
    observe: vi.fn(),
    unobserve: vi.fn(),
    disconnect: vi.fn(),
}));

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
            { name: '$in', type: 'shape' },
            { name: 'path', type: 'string' }
          ],
          outputs: {
            '$out': { type: 'file', mimeType: 'application/pdf' }
          }
        }
      }),
      setError: vi.fn(),
      dynamicOps: vi.fn().mockReturnValue({})
    },
    DEFAULT_CODE: 'Box(10)'
  };
});

// Mock jot compiler
vi.mock('../../jot/src/compiler', () => {
  return {
    JotCompiler: class {
      constructor() {}
      parse() { return { type: 'Program', body: [] }; }
      evaluate() {
        return Promise.resolve([
          { 
            selector: { path: 'jot/Box', parameters: { width: 10, height: 10, depth: 10 }, output: '$out' },
            schema: { type: 'jot:shape' }
          },
          {
            selector: { path: 'foo.pdf', parameters: { path: 'foo.pdf' }, output: 'pdf' },
            schema: { type: 'file' }
          }
        ]);
      }
      dynamicOps() { return {}; }
    }
  };
});

// Mock ThreeUtils and Viewport to avoid WebGL issues in JSDOM
vi.mock('../src/lib/three_utils', () => ({
  packZFS: vi.fn().mockResolvedValue({ type: 'packed-geometry' })
}));

vi.mock('../src/components/viewport/Viewport', () => ({
  Viewport: () => <div data-testid="mock-viewport">Viewport</div>
}));

// Mock blackboard
vi.mock('../src/lib/blackboard', async () => {
  const actual = await vi.importActual('../src/lib/blackboard');
  return {
    ...actual,
    blackboard: {
      ...actual.blackboard,
      readSelectorData: vi.fn().mockResolvedValue(new Uint8Array([1, 2, 3])),
      readSelector: vi.fn().mockResolvedValue({
          stream: new ReadableStream({
              start(controller) {
                  controller.enqueue(new Uint8Array([1, 2, 3]));
                  controller.close();
              }
          }),
          metadata: { state: 'AVAILABLE', encoding: 'bytes' }
      })
    }
  };
});

describe('JotNode Terminal Integration', () => {
  it('should display a download button when terminal files are discovered', async () => {
    const { unmount } = render(() => <JotNode id="test-node" />);
    
    // 1. Find the code editor (text area)
    const editor = screen.getByTestId('jot-code-editor');
    
    // 2. Update code to produce a PDF terminal
    fireEvent.input(editor, { target: { value: 'Box(10).pdf("foo.pdf") -> $out' } });
    
    // 3. Trigger evaluation
    const evaluateBtn = screen.getByText('Evaluate Jot');
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
