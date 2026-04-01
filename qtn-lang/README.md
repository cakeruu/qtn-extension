## QTN Language Extension

Lightweight support for `.qtn` files in VS Code.

### What it does
- Syntax highlighting for QTN keywords, types, attributes, and directives.
- Type/keyword autocompletion and basic snippets.
- Diagnostics from `qtnd` (live while typing + on save).
- Formatting support for QTN files.

### Commands
- `QTN: Show Output`
- `QTN: Restart Qtnd Server`

### How to install
- Download the ZIP file that contains the `.vsix` extension package and the `qtnd` binary.
- Put the `qtnd` binary in any folder you want, then add that folder to your system `PATH` environment variable.
- Install the `.vsix` in VS Code with `Ctrl+P` -> `Extensions: Install from VSIX...`.

### Requirement
- `qtnd` must be installed and available in your `PATH`.
