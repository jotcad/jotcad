import { describe, it } from 'node:test';
import { Session } from './session.js'; // Adjust path if necessary
import assert from 'node:assert/strict';
import path from 'node:path'; // For comparison

// Mock implementation for testing the abstract Session class
class MockSession extends Session {
  constructor(sessionId, rootPath, assets) {
    super(sessionId, rootPath, assets);
  }

  // Implement abstract methods as no-ops for testing purposes
  static async getOrCreateSession(sessionId) {
    return new MockSession(sessionId, '/tmp/mock_session_root', null);
  }

  static getSession(sessionId) {
    return null; // Not needed for this test
  }
}

describe('Session.filePath', () => {
  const mockSessionId = 'test_session_id';
  const mockRootPath = '/mock/root';
  const mockAssets = {}; // Mock assets object
  const sessionInstance = new MockSession(
    mockSessionId,
    mockRootPath,
    mockAssets
  );

  // Manually set the paths property for consistent testing
  sessionInstance.paths = {
    root: mockRootPath,
    assets: path.join(mockRootPath, 'assets'),
    files: path.join(mockRootPath, 'files'),
  };

  it('should resolve a basic relative path within the files directory', () => {
    const relativePath = 'my_file.txt';
    const expectedPath = path.join(sessionInstance.paths.files, relativePath);
    assert.strictEqual(sessionInstance.filePath(relativePath), expectedPath);
  });

  // MODIFIED: This test now asserts that filePath does NOT strip the "files/" prefix.
  it('should treat a "files/" prefixed path as a literal and join it to the files directory', () => {
    const relativePath = 'files/nested/document.pdf';
    const expectedPath = path.join(sessionInstance.paths.files, relativePath); // Expect it to be joined directly
    assert.strictEqual(sessionInstance.filePath(relativePath), expectedPath);
  });

  it('should return the files directory path for an empty relative path', () => {
    const relativePath = '';
    const expectedPath = sessionInstance.paths.files;
    assert.strictEqual(sessionInstance.filePath(relativePath), expectedPath);
  });

  it('should resolve a relative path with subdirectories correctly', () => {
    const relativePath = 'data/images/photo.jpg';
    const expectedPath = path.join(sessionInstance.paths.files, relativePath);
    assert.strictEqual(sessionInstance.filePath(relativePath), expectedPath);
  });

  it('should handle special characters in relative path', () => {
    const relativePath = 'file with spaces & symbols.name';
    const expectedPath = path.join(sessionInstance.paths.files, relativePath);
    assert.strictEqual(sessionInstance.filePath(relativePath), expectedPath);
  });

  it('should handle a dot-relative path (./) correctly', () => {
    const relativePath = './image.png';
    const expectedPath = path.join(sessionInstance.paths.files, 'image.png');
    assert.strictEqual(sessionInstance.filePath(relativePath), expectedPath);
  });

  it('should handle a dot-dot-relative path (../) correctly by resolving within the root of filesPath', () => {
    const relativePath = '../outside.txt';
    const expectedPath = path.join(sessionInstance.paths.files, relativePath); // path.join handles '..'
    assert.strictEqual(sessionInstance.filePath(relativePath), expectedPath);
  });
});

// NEW DESCRIBE BLOCK for resolvePath tests
describe('Session.resolvePath', () => {
  const mockSessionId = 'test_session_id';
  const mockRootPath = '/mock/root';
  const mockAssets = {};
  const sessionInstance = new MockSession(
    mockSessionId,
    mockRootPath,
    mockAssets
  );

  sessionInstance.paths = {
    root: mockRootPath,
    assets: path.join(mockRootPath, 'assets'),
    files: path.join(mockRootPath, 'files'),
  };

  it('should strip "files/" prefix and resolve path within files directory', () => {
    const relativePath = 'files/nested/document.pdf';
    const expectedPath = path.join(
      sessionInstance.paths.files,
      'nested/document.pdf'
    );
    assert.strictEqual(sessionInstance.resolvePath(relativePath), expectedPath);
  });

  it('should resolve path within assets directory if no "files/" prefix', () => {
    const relativePath = 'my_asset.jpg';
    const expectedPath = path.join(sessionInstance.paths.assets, relativePath);
    assert.strictEqual(sessionInstance.resolvePath(relativePath), expectedPath);
  });

  it('should handle mixed prefixes (assets before files)', () => {
    const relativePath = 'assets/my_asset.jpg'; // This won't be caught by files/
    const expectedPath = path.join(sessionInstance.paths.assets, relativePath);
    assert.strictEqual(sessionInstance.resolvePath(relativePath), expectedPath);
  });
});
