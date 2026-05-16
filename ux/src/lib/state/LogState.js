import { createSignal } from 'solid-js';

export const [logs, setLogs] = createSignal([]);

export const logActions = {
  add(msg, type = 'info') {
    setLogs(prev => [...prev, { msg: String(msg), type, t: Date.now() }].slice(-200));
  },
  clear() {
    setLogs([]);
  },
  initInterception() {
    if (typeof window === 'undefined' || window._LOGS_INTERCEPTED) return;
    window._LOGS_INTERCEPTED = true;

    const originalLog = console.log;
    const originalError = console.error;
    const originalWarn = console.warn;

    console.log = (...args) => {
      this.add(args.map(a => typeof a === 'object' ? JSON.stringify(a) : a).join(' '), 'info');
      originalLog.apply(console, args);
    };
    console.error = (...args) => {
      this.add(args.map(a => typeof a === 'object' ? JSON.stringify(a) : a).join(' '), 'error');
      originalError.apply(console, args);
    };
    console.warn = (...args) => {
      this.add(args.map(a => typeof a === 'object' ? JSON.stringify(a) : a).join(' '), 'warn');
      originalWarn.apply(console, args);
    };
  }
};
