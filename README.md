# yabai (macOS 27 fork)

A maintained fork of [yabai](https://github.com/asmvik/yabai) that works on macOS 27: instant space switching with SIP on or off, signed and notarized builds.

[![Latest release](https://img.shields.io/github/v/release/Christian-SC26/yabai?color=green)](https://github.com/Christian-SC26/yabai/releases)
[![License: MIT](https://img.shields.io/github/license/Christian-SC26/yabai.svg?color=green)](LICENSE.txt)

yabai is a tiling window manager for macOS. It arranges windows with binary space partitioning and lets you control windows, spaces and displays from the command line. Bind those commands to keys with [skhd](https://github.com/asmvik/skhd) or a similar tool.

<img align="right" width="40%" src="assets/screenshot.png" alt="Two code editor windows tiled side by side by yabai, with a floating terminal window on top">

**Status:** maintained, with 12 releases between September 22 and October 29, 2026. Commands and config format match upstream. I test fixes on macOS 27.2.

## What's different from upstream

- **Scripting addition on macOS 27.0–27.2.** `sudo yabai --load-sa` works on macOS 27, including the reduced capability mask on 27.2+.
- **Space switching with SIP enabled.** `yabai -m space --focus` works without the scripting addition on macOS 27. Jumps across several spaces land on the right space (7.1.37). The swipe direction follows your natural scrolling setting, or set it with `space_gesture_natural_scrolling`.
- **Focus fixes.** Activating an app switches to its space, even after `space --focus`. Focusing an empty space stays there. Focus and the menu bar stay in sync after Mission Control.
- **Mouse.** BSP splits resize live while you drag (`mouse_bsp_live_resize`). The mouse event tap runs on its own thread, so clicks are not dropped while an app blocks the Accessibility API.
- **Sessions and layouts.** `yabai -m space --save-session`, `--load-session`, `--save-layout` and `--load-layout` save and restore spaces and window trees.
- **Signed and notarized releases.** Each release binary carries a Developer ID signature and Apple notarization.

Full list with issue links: [CHANGELOG.md](CHANGELOG.md).

## Install

With Homebrew:

```sh
brew install christian-sc26/formulae/yabai
yabai --start-service
```

macOS asks for Accessibility access on first launch. Grant it in System Settings → Privacy & Security → Accessibility, then run `yabai --restart-service`.

Use the full name `christian-sc26/formulae/yabai` in every brew command. If the upstream tap `asmvik/formulae` is also installed, `brew upgrade yabai` stops with "Formulae found in multiple taps".

To upgrade:

```sh
brew upgrade christian-sc26/formulae/yabai
yabai --restart-service
```

Each upgrade installs the binary under a new versioned path. macOS treats it as a new program, so you have to grant Accessibility access again.

### Switching from upstream yabai

```sh
yabai --stop-service
brew uninstall yabai
brew install christian-sc26/formulae/yabai
yabai --start-service
```

### Without Homebrew

Universal binaries (Apple Silicon and Intel) are on the [releases page](https://github.com/Christian-SC26/yabai/releases). The install script downloads the current release and checks its SHA-256:

```sh
curl -fsSL https://raw.githubusercontent.com/Christian-SC26/yabai/master/scripts/install.sh | sh
```

### Scripting addition (optional)

Some features, such as moving spaces or changing window layers, need the scripting addition injected into Dock.app. That requires [partially disabling SIP](https://github.com/asmvik/yabai/wiki/Disabling-System-Integrity-Protection). Then:

```sh
sudo yabai --load-sa
```

To load it without a password prompt and after every Dock restart, follow [Configure scripting addition](https://github.com/asmvik/yabai/wiki/Installing-yabai-(latest-release)#configure-scripting-addition) in the upstream wiki.

## Usage

yabai reads `~/.config/yabai/yabairc` on start. Copy the example config as a starting point:

```sh
mkdir -p ~/.config/yabai
cp "$(brew --prefix)/share/yabai/examples/yabairc" ~/.config/yabai/yabairc
```

Common commands:

```sh
yabai -m config layout bsp          # tile windows on all spaces
yabai -m window --focus west        # focus the window to the left
yabai -m space --focus 3            # go to space 3
yabai -m window --space 2           # move the focused window to space 2
yabai -m query --windows --window   # print the focused window as JSON
```

Every command and setting is documented in [doc/yabai.asciidoc](doc/yabai.asciidoc) (`man yabai`). The [upstream wiki](https://github.com/asmvik/yabai/wiki) covers configuration and common setups.

## Requirements and caveats

- macOS 11 Big Sur or later on Intel, macOS 12 Monterey or later on Apple Silicon.
- System Settings → Desktop & Dock → Mission Control: turn **on** "Displays have separate Spaces" and turn **off** "Automatically rearrange Spaces based on most recent use".
- Window animations need Screen Recording permission. Nothing else does.
- Do not disable the Finder desktop (`defaults write com.apple.finder CreateDesktop`). Without it, empty spaces cannot be focused.
- Native tabs in NSDocument apps such as Finder and Terminal [do not tile correctly](https://github.com/asmvik/yabai/issues/68). Avoid tabs there, or make those windows float with a rule.
- Disabling SIP lowers the security of your Mac. Read [Apple's notes on SIP](https://support.apple.com/en-us/HT204899) before you do it. The core features work with SIP enabled.

## When to use upstream instead

If you are not on macOS 27 and upstream yabai works for you, stay with [asmvik/yabai](https://github.com/asmvik/yabai). Its author maintains the project and the wiki. This fork follows upstream and adds fixes on top.

## License and credits

MIT, see [LICENSE.txt](LICENSE.txt). [Åsmund Vikane](https://github.com/asmvik) created and maintains yabai. [@fools-mate](https://github.com/fools-mate) drew the logo and banner, and [@dominiklohmann](https://github.com/dominiklohmann) wrote much of the documentation.
