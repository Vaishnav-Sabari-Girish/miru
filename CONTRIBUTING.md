# Contributing to `miru`

Thank you for your interest in wanting to contribute to `miru`.

> [!NOTE]
> Development is done on
> [Codeberg](https://codeberg.org/Vaishnav-Sabari-Girish/miru) with a mirror in
> [GitHub](https://github.com/Vaishnav-Sabari-Girish/miru)

## Before you start

1. Take a look at the [`README`](./README.md) (especially the **Requirements**,
   **Building** and **Roadmap** sections)
2. Check open issues so you do not duplicate existing work
3. For larger changes, please open an issue first and after approval from the
   maintainer, go for a PR.

## Development Setup

### Build dependencies

- `cmake` (>= 3.20)
- A C compiler with at least **C11** (`cmake` prefers C23 when available)
- `wayland-client`, `wayland-protocols`, `wayland-scanner`
- EGL + OpenGL ES 2 development packages
- Optional: `ninja`
- Optional: [Grimoire](https://github.com/Vaishnav-Sabari-Girish/grimoire)

```bash
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```

Or using Grimoire

```bash
grim cast build
```

Binaries: `build/miru-daemon` and `build/miructl`

### Contributor tools (formatting / lint)

Install these so local checks match the CI

| Tools | Purpose |
| -------------- | --------------- |
| `clang-format` | Format the C code based on the `clang-format` file |
| `cmake-format` | Format the `CMakeLists.txt` and other `.cmake` files based on the `.cmake-format.yaml` file |
| `clang-tidy` | For linting the C code |
| `typos` | Spell-check |
| `rumdl` | Markdown lint/format |

The CI (Woodpecker) runs the same checks using the above tools (Check
`.woodpecker/` directory)

### Linting the code

Use `clang-tidy` for code linting. The linting rules are present in the
`.clang-tidy` file.

```bash
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
clang-tidy -p build --quiet $(find src -type f -name '*.c' | sort)
```

### Git Hooks

Hooks live in `.githooks/` (Not the default `.git/hooks`). Point `git` at that
directory once per clone

```bash
git config core.hooksPath .githooks
```

The pre-commit hook typically:

- runs `typos` (Spell-check)
- runs `clang-format` (Formats the C code)
- run `clang-tidy` (Lints the code)
- runs `cmake-format` (Formats the `CMakeLists.txt` and `.cmake` files)
- runs `rumdl` on Markdown (Excluding a few files)

And stages all the above changes

Make the hook executable (If needed)

```bash
chmod +x .githooks/pre-commit
```

### Coding Guidelines

- **Language**: C (C11 minimum, but C23 is preferred)
- **Style**: follow existing code. The hook automatically runs the formatters,
  but run `clang-format`, `cmake-format`, `typos` and `rumdl` individually once
  to prevent any issues.
- Prefer small, focused commits and clear messages which follow the
  [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0-beta.3/)
  specification.
- Do not break supported compositors (`niri`, `hyprland`, `mangowm`, and other
  `wl-roots` based ones)
- Keep the "_freeze on toggle_" model unless tested thoroughly on all the above
  mentioned compositors.
- Avoid drive-by refactors unrelated to the PR.

### Testing your change

1. Build with a clean tree
2. Run `miru-daemon` in debug mode (preferred for more detailed logs)

```bash
MIRU_DEBUG=1 ./build/miru-daemon
```

3. Exercise the paths you touched (e.g: `miructl toggle` / `loupe` / `quit`,
   annotate, help, refresh, cursor toggle)
4. Confirm formatting/linting hooks pass before opening a PR.

## Pull Requests

1. Target **Codeberg** `main`
2. Describe what is changed and why. Also link an issue if you have opened one.
3. Keep the diff limited to the feature/fix.
4. Update **`README`**, `man` pages (`.scd`), or **help** text when behavior or
   keybinds are changed.
5. CI should not fail (format, spell-check, markdown)

## Project Layout (Where to look)

```text
src/           daemon sources (Wayland, GL, input, config, annotations, …)
ctl/           miructl client
cmake/         CMake helpers (e.g. WaylandScanner.cmake)
protocol/      vendored protocol XML
.woodpecker/   CI pipelines
.githooks/     Git hooks (enable via core.hooksPath)
```

See the [**Project Structure**](./README.md#project-structure) section in the
**README** for a more detailed tree.

## LICENSE

By contributing, you agree that your contributions are licensed under the
same terms as the project (see [LICENSE](./LICENSE)).
