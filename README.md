# ang.sh — angsh

<p align="center">
  <img src="https://img.shields.io/badge/version-0.1.0-blueviolet?style=flat-square" alt="version">
  <img src="https://img.shields.io/badge/language-C%2B%2B17-00599C?style=flat-square" alt="C++17">
  <img src="https://img.shields.io/badge/dependencies-none-brightgreen?style=flat-square" alt="no deps">
  <img src="https://img.shields.io/badge/license-MIT-yellow?style=flat-square" alt="MIT">
  <img src="https://img.shields.io/badge/platform-linux%20%7C%20macOS-lightgrey?style=flat-square" alt="platform">
</p>

```
   __ _ _ __   __ _ ___| |__
  / _` | '_ \ / _` / __| '_ \
 | (_| | | | | (_| \__ \ | | |
  \__,_|_| |_|\__, |___/_| |_|
               |___/  v0.1.0
```

A small Unix shell in C++ that takes the best ideas from **bash**, **zsh**,
and **fish** — with two syntax modes, **Ang** (modern, default) and **POSIX**
(compatible) — and builds them into something you can live in and style.

## ✨ The look

```
✦ armaans@0x  ~/Code/ang.sh  ⎇ main  ❯                               21:18 ✓
```

That's the `rice` theme. `default`, `minimal`, `power`, and `mono` also ship.
The prompt renderer understands:

| Kind | Placeholders |
|---|---|
| **segments** | `{user}` `{host}` `{cwd}` `{base}` `{status}` `{char}` `{git}` `{branch}` `{mode}` `{time}` |
| **status art** | `{ok}` ✓ / `{fail}` ✗ code / `{arrow}` ❯ that turns red on failure |
| **color** | `{red}`…`{white}`, `{br_red}`…`{br_white}`, 256-color `{orange}` `{purple}` `{pink}` `{gray}` `{teal}` `{lime}` |
| **backgrounds** | `{on_black}` `{on_red}` `{on_green}` `{on_yellow}` `{on_blue}` `{on_magenta}` `{on_cyan}` `{on_white}` |
| **style** | `{dim}` `{bold}` `{reset}` |

**Theme gallery**

| Theme | Looks like | Notes |
|---|---|---|
| `default` | `armaans@0x ~/Code/ang.sh ⎇ main ❯` … `21:18` | balanced, 256-color accents |
| `minimal` | `~/Code ❯` … `✗ 1` | status only when it matters |
| `rice` | `✦ armaans@0x  ~/Code/ang.sh  ⎇ main  ❯` … `21:18 ✓` | maximum shimmer, no fonts needed |
| `power` | `▊ armaans@0x ▸ ang.sh ❯` | powerline segments — needs a Nerd Font |
| `mono` | `armaans@0x ~/Code/ang.sh $` | zero color: logs, dumb terminals, purists |

```sh
theme rice     # switch live, or set it in ~/.config/angsh/config.ang
```

## 🍬 What it borrowed

| Feature | From | In v0.1.0 |
|---|---|---|
| pipes, `>`, `>>`, `<`, `2>`, `&>`, `&&`, `\|\|`, `;`, `&` | bash/POSIX | ✅ |
| `$VAR`, `${VAR}`, `$?`, `$$`, `$!`, `~`, glob `* ? []` | bash | ✅ |
| history file, `!!` / `!$`, `alias`, `type` | bash | ✅ |
| left/right prompt themes, `autocd`, plugin dir | zsh | ✅ |
| ghost autosuggestions, inline highlighting, `abbr`, `set` syntax | fish | ✅ |
| `if`/`for`/`while`, functions, `$(...)` | — | ⏳ v0.2 |

The interactive niceties are all there out of the box:

- **Ghost suggestions** — type `g`, see the rest of the last `git …` command in
  dim text, hit `→` or `Ctrl-F` to accept.
- **Inline highlighting** — commands are green when found, red when missing,
  strings yellow, flags cyan.
- **A welcome you'll actually look at** — the banner greets you with mode,
  version, and active theme.

## 🔀 Two syntaxes

Default is **Ang**. Pass `--posix` (or `ANG_POSIX=1`) for POSIX mode.

| Task | Ang (default) | POSIX (`posix on`) |
|---|---|---|
| set a variable | `set FOO bar baz` | `export FOO="bar baz"` |
| temp env for one cmd | use `export` | `FOO=bar cmd` |
| read a variable | `$FOO` (both) | `$FOO` |
| toggle mode | `posix on` / `posix off` | same |
| script shebang | `#!/usr/bin/env angsh` | `#!/usr/bin/env angsh --posix` |

In Ang mode, a bare `FOO=bar` gets a friendly red hint instead of silently
guessing what you meant:

```
angsh: assignment 'FOO=bar' needs POSIX mode (try `set FOO bar` or `posix on`)
```

## 📦 Install

**One-liner** — resolves the latest release, verifies the checksum, and installs
the static binary (`/usr/local/bin` if writable, else `~/.local/bin`):

```sh
curl -fsSL https://raw.githubusercontent.com/0x251C08/ang.sh/main/install.sh | sh
```

Pin a version or pick a directory:

```sh
curl -fsSL https://raw.githubusercontent.com/0x251C08/ang.sh/main/install.sh | sh -s -- --version v0.1.0 --dir ~/bin
```

**Manual download** — Linux x86_64, fully static (no runtime dependencies):

```sh
curl -LO https://github.com/0x251C08/ang.sh/releases/download/v0.1.0/angsh-v0.1.0-x86_64-linux
curl -LO https://github.com/0x251C08/ang.sh/releases/download/v0.1.0/angsh-v0.1.0-x86_64-linux.sha256
sha256sum -c angsh-v0.1.0-x86_64-linux.sha256
chmod +x angsh-v0.1.0-x86_64-linux
sudo mv angsh-v0.1.0-x86_64-linux /usr/local/bin/angsh
```

**From source** — any POSIX system with a C++17 compiler and CMake 3.16+:

```sh
cmake -B build && cmake --build build   # optimized (Release) by default
./build/angsh
sudo cmake --install build              # /usr/local/bin + themes
```

Use `-DCMAKE_BUILD_TYPE=Debug` for a debug build.
**Zero libraries** — stdlib and POSIX only, by design.

## 🚀 Usage

```sh
angsh                    # interactive (Ang mode)
angsh --posix            # interactive (POSIX mode)
angsh -c 'echo hi | tr a-z A-Z'
angsh script.ang         # run a script
```

## 🎨 Ricing

Every file below is either plain angsh commands or `key="value"` lines — no
new config language to learn:

```sh
~/.config/angsh/config.ang      # plain commands (see examples/config.ang)
~/.config/angsh/theme.conf      # prompt_left / prompt_right
~/.config/angsh/themes/*.conf   # your own themes
~/.config/angsh/plugins/*.ang   # sourced alphabetically at startup
```

A theme is three lines of honesty:

```ini
prompt_left="{orange}{user}{reset}{gray}@{reset}{teal}{host}{reset} {bold}{cwd}{reset}{purple}{branch}{reset} {arrow} "
prompt_right="{gray}{mode}{time}{fail}{reset}"
```

Interactive keys: `Tab` complete · `→`/`Ctrl-F` accept suggestion ·
`↑`/`↓` history · `Ctrl-A/E/U/K/W` edit · `Ctrl-C` cancel · `Ctrl-D` exit.

> Note: `alias`/`abbr` expand once per input *line* (same as bash). In
> `angsh -c '...'` strings, put aliased commands on separate lines
> (`-c $'alias ll=... \nll'`) or prefer scripts, where each line expands.

## 🧑‍💻 For developers

```
src/
  main.cpp      CLI + REPL loop (~150 lines)
  lexer.cpp     tokenizer          parser.cpp   && || | ; & grammar
  expand.cpp    $VAR ~ glob        executor.cpp fork/pipe/redirect runner
  builtins.cpp  18 builtins        prompt.cpp   theme rendering
  config.cpp    config+plugins     editor.cpp   raw-mode line editor
  highlight.cpp colors+suggest     env/util     state + helpers
```

Add a builtin in 3 steps (documented in `src/builtins.h`):
declare → define → register in `is_parent_builtin`/`run_builtin`.

Colors respect `NO_COLOR` and `TERM=dumb`, so piped output and CI logs stay
clean; `visible_len()` skips ANSI escapes and UTF-8 continuation bytes so
right prompts stay pixel-aligned.

## 🗺️ Roadmap

- **v0.2** — `if`/`for`/`while` blocks (both syntaxes), functions, `$(...)`, completion scripts
- **v0.3** — vi mode, async prompt, `~/.config/angsh/completions/`
- **v1.0** — POSIX test-suite parity for the `--posix` subset

## 📄 License

[MIT](LICENSE) — do what you want, just keep the notice.
