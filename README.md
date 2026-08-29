# 3FC

**by FerdinandoPH** — English · [Español](README.es.md)

**3FC** (*3DS FTP Client*) is a graphical FTP client that runs on the Nintendo 3DS
itself. Its main use is moving files **directly between two 3DS consoles** — one
running 3FC, the other running [ftpd](https://github.com/mtheall/ftpd) — with no PC
in the middle, but it talks to any FTP server just as well.

It is written in C++20 with [devkitPro](https://devkitpro.org/) and
[Dear ImGui](https://github.com/ocornut/imgui), speaks plain FTP in passive mode
(PASV), always transfers in binary, and handles UTF-8, so accented filenames display
and transfer correctly.

## Features

- **5 connection slots** saved on the SD card: user, password, *anonymous* checkbox
  and an optional alias, typed with the console's native keyboard.
- **Dual browser**: SD card on one side, server on the other, swapped with `L`/`R`.
- **Transfer queue** with progress bar, ETA and cancellation; folders transfer
  recursively and existing files prompt before being overwritten.
- **Multiple selection** and an actions menu: new folder, rename, delete, paste and
  transfer between machines.
- **Console tab** showing the raw FTP dialogue, command by command.
- **English and Spanish UI**, following the console language and switchable on the fly.
- **Console details**: sleep is blocked while a session is open, the backlight can be
  turned off without interrupting transfers, and a New 3DS runs at 804 MHz.

## Controls

Everything is driven with the buttons; the touch keyboard is only used for text entry.
The same help lives inside the app, under `START` → *Controls*.

**Anywhere**

| Button | Action |
|---|---|
| `X` | Switch which screen the buttons act on. The active screen has a blue border; the other one is dimmed. |
| `START` | Open the menu. |
| D-pad / circle pad | Move. Left and right jump a page. |

**Top screen — connection**

| Button | Action |
|---|---|
| `A` | Connect, or fill an empty slot. |
| `Y` | Edit the slot. |

**Top screen — file browser**

| Button | Action |
|---|---|
| `A` | Enter a folder. |
| `B` | Go up one level. |
| `L` / `R` | Switch between the SD card and the server. |
| `SELECT` | Mark or unmark the item under the cursor. |
| `Y` | Actions: new folder, rename, delete, paste, transfer. |

**Bottom screen**

| Button | Action |
|---|---|
| `L` / `R` | Switch between the Transfers and Console tabs. |
| `A` (Transfers) | Cancel the transfer in progress. |
| `Y` (Transfers) | Cancel every transfer still waiting. |
| D-pad (Console) | Scroll the FTP log. It stops following new lines while you read back. |

The `START` menu turns off the backlight (transfers keep running; any button turns it
back on), changes language, disconnects, shows these controls, shows the licences and
exits.

## Install

Copy `3FC.3dsx` to `sdmc:/3ds/` and launch it from the Homebrew Launcher, or install
`3FC.cia` with FBI to get it in the HOME menu.

Connection slots are stored in `sdmc:/3ds/3FC/config.cfg`, created on first run.

> Slot passwords are saved **in plain text**: the 3DS has no secret store, and the SD
> card is readable by anyone with physical access to the console.

## Development

You need devkitPro with `devkitARM`, `libctru` and `citro3d`
(`dkp-pacman -S 3ds-dev`) and `DEVKITARM` exported. `make cia` additionally needs
`bannertool` and `makerom` on the `PATH`.

```sh
make            # 3FC.3dsx + 3FC.smdh
make cia        # also 3FC.cia
make DEBUG=1    # -Og -g3, readable symbols
make clean
```

Publishing a GitHub release runs `.github/workflows/release.yml`, which builds in the
devkitPro container and attaches `3FC.3dsx`, `3FC.smdh` and `3FC.cia` to it.

To try a build on hardware, open the Homebrew Launcher on the console and run
`3dslink 3FC.3dsx`; an emulator such as Azahar also loads the `.3dsx` directly.

Layout: `source/` holds the app — `net/` (non-blocking sockets), `ftp/` (protocol and
session), `fs/` (local and remote file operations), `transfer/` (the queue), `ui/`
(ImGui screens and panels), `i18n/` and `config/`. `third_party/` vendors Dear ImGui
and its 3DS backend, and `meta/` holds the CIA banner and RSF.

The icon and the CIA banner image are regenerated with
`python3 meta/makeart.py icon.png meta/banner.png`.

## Licence

3FC is **GPL-3.0** — see [`LICENSE`](LICENSE).

Everything under `third_party/` is MIT and keeps its original licence header: Dear
ImGui 1.91.8 © Omar Cornut ([`third_party/imgui/LICENSE.txt`](third_party/imgui/LICENSE.txt)),
and the 3DS ImGui backend (`imgui_citro3d`, `imgui_ctru`, `vshader.v.pica`)
© 2020/2024 Michael Theall, taken from [ftpd](https://github.com/mtheall/ftpd).
