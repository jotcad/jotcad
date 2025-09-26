let fs = null;

export const withFs = async (newFs, op) => {
  try {
    fs = newFs;
    await op();
  } finally {
    fs = null;
  }
};

export const readFile = (...args) => fs.readFile(...args);
export const writeFile = (...args) => fs.writeFile(...args);
