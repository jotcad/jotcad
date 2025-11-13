import { mkdir, rename } from 'node:fs/promises';

import { createHash } from 'node:crypto';
import path from 'node:path';

const SESSIONS_DIR = '/tmp/jotcad/sessions';
const ONE_HOUR_IN_MS = 60 * 60 * 1000;

const sessions = new Map();

export const getOrCreateSession = async (sessionId) => {
  if (sessions.has(sessionId)) {
    return sessions.get(sessionId);
  }
  const sessionPath = path.join(SESSIONS_DIR, createHash('sha256').update(sessionId).digest('hex'));
  const assetsPath = path.join(sessionPath, 'assets');
  const filesPath = path.join(sessionPath, 'files');
  const session = {
    id: sessionId,
    path: sessionPath,
    assetsPath: assetsPath,
    filesPath: filesPath,
    createdAt: Date.now(),
  };
  sessions.set(sessionId, session);
  await mkdir(assetsPath, { recursive: true });
  await mkdir(filesPath, { recursive: true });
  return session;
};

export const getSession = (sessionId) => sessions.get(sessionId);

const isSessionExpired = (session) => {
  const now = Date.now();
  return (now - session.createdAt) > ONE_HOUR_IN_MS;
};

export const cleanupSessions = async () => {
  for (const [sessionId, session] of sessions.entries()) {
    if (isSessionExpired(session)) {
      try {
        const newPath = session.path + '.deleteme';
        await rename(session.path, newPath);
        sessions.delete(sessionId);
        console.log(`Marked stale session for deletion: ${sessionId} (renamed to ${newPath})`);
      } catch (error) {
        console.error(`Error marking session ${sessionId} for deletion:`, error);
      }
    }
  }
};

export const startCleanup = () => {
  setInterval(cleanupSessions, ONE_HOUR_IN_MS);
  console.log('Session cleanup process started.');
};
