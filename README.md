# WriteCam

A personal fork of Omawrite: a dead-simple Markdown writing app built with Qt Quick and C++ that automatically follows system dark/light mode, extended to maintain YAML front matter on save.

> **Live Web Demo:** Try the interactive demo on GitHub Pages at [https://razodin137.github.io/omawrite-personal/](https://razodin137.github.io/omawrite-personal/).

<img width="2948" height="3227" alt="screenshot-2026-06-23_15-24-08" src="https://github.com/user-attachments/assets/4e930c0d-edda-4046-b444-a59eff523329" />
<img width="2948" height="3227" alt="screenshot-2026-06-23_15-23-23" src="https://github.com/user-attachments/assets/8ced7c26-961b-4ded-b263-84403001a951" />


## Install

> **Note:** The one-click installer and AUR package are **specifically for Arch Linux** and Arch-based distributions (EndeavourOS, Manjaro, CachyOS, etc.) as they depend on `pacman` and `makepkg`.

### One-Click Install (Arch Linux only)

Run in your terminal:

```sh
curl -fsSL https://razodin137.github.io/omawrite-personal/install.sh | sh
```

*(Or via GitHub raw: `curl -fsSL https://raw.githubusercontent.com/razodin137/omawrite-personal/master/install.sh | sh`)*

### From Git (Arch Linux)

```sh
git clone https://github.com/razodin137/omawrite-personal.git && cd omawrite-personal && ./bin/install
```

### Arch User Repository (AUR)

Once registered on the AUR, you can install with any AUR helper:

```sh
yay -S writecam-git
# or
paru -S writecam-git
```

### Manual Build / Other Linux Distributions

If you are running another Linux distribution (Ubuntu, Fedora, Debian) or prefer to run without installing system-wide:

1. Install the required dependencies:
   - Qt 6 development libraries (`qt6-base`, `qt6-declarative`, `qt6-quickcontrols2`)
   - `xdg-desktop-portal`
   - C++ compiler (`gcc` or `clang`) and `make` / `qmake6`
2. Build and run from the checkout:
   ```sh
   ./bin/build
   ./build/writecam
   ```

## Shortcuts

- `Ctrl+S` saves. Unsaved documents use the XDG desktop portal file picker.
- `Ctrl+Shift+S` saves as.
- `Ctrl+O` opens a Markdown file through the portal picker.
- `Ctrl+P` opens the system print dialog.
- `Ctrl+N` opens a new WriteCam window.
- `Ctrl+Z`, `Ctrl+Shift+Z`, and `Ctrl+Y` handle undo and redo.
- `Super+F` toggles fullscreen. Qt maps this key as `Meta+F`.
- `Ctrl+F` searches the document. Use `Enter` or `Ctrl+G` for the next match and `Shift+Enter` for the previous match.
- `Ctrl+H` opens find and replace.
- `Ctrl+B`, `Ctrl+I`, and `Ctrl+K` insert bold, italic, and link Markdown.
- `Ctrl+?` shows the keyboard shortcut reference.

Unsaved drafts are recovered after an abnormal exit. WriteCam also watches open files
and warns before an external change can replace local work.

Saving a document for the first time writes YAML front matter at the top of the file —
the creation `date`, an empty `updated` log, the `title` taken from the first heading
(or line) of the document, `draft: true`, and empty `categories` and `tags` lists:

```yaml
---
date: 2026-10-12
updated: []
title: My new post
draft: true
categories: []
tags: []
---
```

The front matter is visible in the editor, so the fields can be edited like any other
text. Every save of an edited document appends that save's timestamp to the `updated`
log, so a post revised through the day keeps its history:

```yaml
updated:
  - 2026-10-12 14:23
  - 2026-10-12 16:45
```

Front matter that already exists keeps every field exactly as written — WriteCam only
maintains the log. A file whose front matter doesn't track `updated` has the log
started (right after its `date` line) on the first edited save; plain files without
front matter are never modified.

New documents suggest a filename of the day's date plus a hyphenated slug of the first
heading (or line) — `# My Awesome Day` becomes `2026-10-12-my-awesome-day.md` — unless
the title already starts with a date.

The font button in the footer picks the writing font from any installed text font,
and WriteCam remembers the choice. IBM Plex Mono is the default.

Text follows the desktop text size — `omarchy display text size`, or GNOME's
`text-scaling-factor` — and re-flows without a restart. The default of 12px leaves
WriteCam at the size it is designed around; larger and smaller sizes scale from there.

## Requirements

- Qt 6: `qt6-base`, `qt6-declarative`, `qt6-quickcontrols2`
- `xdg-desktop-portal` and a portal backend

The IBM Plex Mono font is bundled under the SIL Open Font License 1.1; see
`fonts/OFL.txt`. The font is copyright IBM Corp.
