import { spawn } from 'node:child_process';
import fs from 'node:fs';

/**
 * Robustly spawns the C++ Native Ops node and waits for it to become healthy.
 * @param {string} opsPath Absolute path to the ops binary.
 * @param {string[]} args Arguments for the binary.
 * @param {number} healthPort The port to check for health.
 * @param {Object} options Additional options (env, etc).
 * @returns {Promise<Object>} The spawned process object.
 */
export async function spawnOpsNode(opsPath, args, healthPort, options = {}) {
  const logFile = fs.createWriteStream('ops_integration.log', { flags: 'a' });
  
  logFile.write(`\n--- [${new Date().toISOString()}] Spawning Ops Node on port ${healthPort} ---\n`);
  logFile.write(`Command: ${opsPath} ${args.join(' ')}\n`);

  const opsProcess = spawn(opsPath, args, {
    env: {
      ...process.env,
      ...options.env,
    },
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  let stdout = '';
  let stderr = '';

  opsProcess.stdout.on('data', (data) => {
    const s = data.toString();
    stdout += s;
    logFile.write(`[STDOUT] ${s}`);
  });
  opsProcess.stderr.on('data', (data) => {
    const s = data.toString();
    stderr += s;
    logFile.write(`[STDERR] ${s}`);
  });

  let processError;
  opsProcess.on('error', (err) => {
    processError = err;
    logFile.write(`[ERROR] ${err.message}\n`);
  });

  let exitCode = null;
  opsProcess.on('exit', (code) => {
    exitCode = code;
    logFile.write(`[EXIT] Code: ${code}\n`);
  });

  const getFullErrorReport = (baseMsg) => {
    let msg = baseMsg;
    if (stdout) msg += `\n--- STDOUT ---\n${stdout.slice(-2000)}`;
    if (stderr) msg += `\n--- STDERR ---\n${stderr.slice(-2000)}`;
    msg += `\n(Check ops_integration.log for full details)`;
    return msg;
  };

  // Wait for node to be healthy
  let healthy = false;
  const maxRetries = 50;
  for (let i = 0; i < maxRetries; i++) {
    if (processError) {
      throw new Error(
        getFullErrorReport(`C++ Node failed to launch: ${processError.message}`)
      );
    }
    if (exitCode !== null && exitCode !== 0) {
      throw new Error(
        getFullErrorReport(`C++ Node exited prematurely with code ${exitCode}`)
      );
    }

    try {
      // Reverted to HTTP
      const resp = await fetch(`http://localhost:${healthPort}/health`);
      if (resp.ok) {
        const info = await resp.json();
        if (info.status === 'OK') {
          healthy = true;
          break;
        }
      }
    } catch (e) {
      // Expected while starting
    }
    await new Promise((resolve) => setTimeout(resolve, 200));
  }

  if (!healthy) {
    opsProcess.kill();
    throw new Error(
      getFullErrorReport(
        `C++ Node at http://localhost:${healthPort} failed to become healthy within ${
          maxRetries * 0.2
        } seconds`
      )
    );
  }

  return opsProcess;
}

/**
 * Standard launcher for integration tests.
 * @param {string} opsPath 
 * @param {number} port 
 * @param {string} storageDir 
 */
export async function launchOpsNode(opsPath, port, storageDir) {
  const child = await spawnOpsNode(opsPath, [port.toString(), storageDir], port);

  return {
    child,
    stop: async () => {
      if (child.exitCode !== null) return;
      
      console.log(`[Ops Helper] Sending SIGTERM to PID ${child.pid}...`);
      child.kill('SIGTERM');
      
      const exitPromise = new Promise(resolve => child.on('exit', resolve));
      const timeoutPromise = new Promise(resolve => setTimeout(() => {
          console.warn(`[Ops Helper] PID ${child.pid} did not exit within 5s, sending SIGKILL...`);
          child.kill('SIGKILL');
          resolve();
      }, 5000));

      await Promise.race([exitPromise, timeoutPromise]);
    }
  };

}
