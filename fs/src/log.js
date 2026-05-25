
const Levels = {
    DEBUG: 0,
    INFO: 1,
    WARN: 2,
    ERROR: 3
};

const getLogLevel = () => {
    if (typeof process === 'undefined') return Levels.INFO;
    const level = process.env.LOG_LEVEL?.toUpperCase();
    if (Levels[level] !== undefined) return Levels[level];
    if (process.env.VERBOSE === '1' || process.env.DEBUG?.includes('jot')) return Levels.DEBUG;
    return Levels.INFO;
};

const currentLevel = getLogLevel();

export const debug = (...args) => {
    if (currentLevel <= Levels.DEBUG) console.log(...args);
};

export const info = (...args) => {
    if (currentLevel <= Levels.INFO) console.log(...args);
};

// Map existing 'log' to 'debug' because most current 'log' calls are high-volume traces
export const log = debug; 

export const warn = (...args) => {
    if (currentLevel <= Levels.WARN) console.warn(...args);
};

export const error = (...args) => {
    if (currentLevel <= Levels.ERROR) console.error(...args);
};
