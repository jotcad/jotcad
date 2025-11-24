import { Assets } from '../geometry/assets.js';
import { FilesystemSession } from '../ops/FilesystemSession.js';
import path from 'node:path';

// Keep getSession, cleanup, and startCleanup as direct delegates
export const getSession = FilesystemSession.getSession;
export const cleanupSessions = FilesystemSession.cleanupSessions;
export const startCleanup = FilesystemSession.startCleanup;

// Override getOrCreateSession to inject the Assets instance
export const getOrCreateSession = async (sessionId) => {
  // First, get the basic session setup from FilesystemSession
  const session = await FilesystemSession.getOrCreateSession(sessionId);

  // If assets are already populated (e.g., from an existing session), do nothing.
  if (session.assets) {
    return session;
  }

  // Otherwise, create and assign the Assets instance.
  // The path for assets is determined by the session itself.
  const assetsPath = session.paths.assets;
  session.assets = new Assets(assetsPath);

  return session;
};
