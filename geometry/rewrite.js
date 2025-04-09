export const rewrite = (node, op) => op(node, (node) => rewrite(node, op));
