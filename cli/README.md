# JotCAD CLI

The `@jotcad/cli` package is a command-line interface for compiling Jot CAD scripts, evaluating geometry operations, and exporting file formats over the Zenoh mesh.

## Installation

The CLI package is integrated inside the workspaces block of the root project. You can run it via `npm` or directly by invoking the executable:

```bash
# Run local CLI
node cli/cli.js <input.jot> [options]
```

## Options

* **`-i, --input <name=val>`**: Bind input variables to global constants inside the Jot script (e.g. `-i size=25`). Can be specified multiple times.
* **`-o, --output <name=path>`**: Map global output variables in the Jot script to target file paths on disk (e.g. `-o stl_file=cube.stl`). Can be specified multiple times.
* **`-g, --gateway <url>`**: Zenoh router gateway URL. If omitted, the CLI will auto-discover active gateways on candidate ports (`9000`, `9200`, `9092`).
* **`-h, --help`**: Display options and usage help.

## Example

```bash
# 1. Create a script file: model.jot
cat << 'EOF' > model.jot
shape = Box(size, size, size).cut(Orb(12));
shape.stl() -> stl_file;
shape.png() -> png_file;
EOF

# 2. Run the CLI against the live mesh router
node cli/cli.js model.jot -i size=25 -o stl_file=cube.stl -o png_file=preview.png
```
