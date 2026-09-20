<!-- Please be careful editing the below HTML, as GitHub is quite finicky with anything that looks like an HTML tag in GitHub Flavored Markdown. -->
<p align="center">
  <img width="75%" src="assets/banner/banner.svg" alt="Banner">
</p>
<p align="center">
  <b>Tiling window management for the Mac.</b>
</p>
<p align="center">
  <a href="https://github.com/asmvik/yabai/blob/master/LICENSE.txt">
    <img src="https://img.shields.io/github/license/asmvik/yabai.svg?color=green" alt="License Badge">
  </a>
  <a href="https://github.com/asmvik/yabai/blob/master/doc/yabai.asciidoc">
    <img src="https://img.shields.io/badge/view-documentation-green.svg" alt="Documentation Badge">
  </a>
  <a href="https://github.com/asmvik/yabai/wiki">
    <img src="https://img.shields.io/badge/view-wiki-green.svg" alt="Wiki Badge">
  </a>
  <a href="https://github.com/asmvik/yabai/blob/master/CHANGELOG.md">
    <img src="https://img.shields.io/badge/view-changelog-green.svg" alt="Changelog Badge">
  </a>
  <a href="https://github.com/asmvik/yabai/releases">
    <img src="https://img.shields.io/github/commits-since/asmvik/yabai/latest.svg?color=green" alt="Version Badge">
  </a>
</p>

> [!IMPORTANT]
> ### 🚀 Community Maintenance Fork (macOS 27 Golden Gate & Critical Fixes)
> This repository is an actively maintained fork created to provide timely fixes, critical stability improvements, and **macOS 27 (Golden Gate)** compatibility in the absence of upstream maintainer activity.
>
> **Issues resolved in this fork:**
> * **macOS 27 (Golden Gate) Full Compatibility** ([#2846](https://github.com/asmvik/yabai/issues/2846), [#2844](https://github.com/asmvik/yabai/issues/2844), [#2841](https://github.com/asmvik/yabai/issues/2841), [#2840](https://github.com/asmvik/yabai/issues/2840), [#2837](https://github.com/asmvik/yabai/issues/2837), [#2824](https://github.com/asmvik/yabai/issues/2824)):
>   * Full support for instant space switching (`yabai -m space --focus <index>`) on macOS 27;
>   * Updated Mach-O opcode scanner and payload injection for macOS 27 `Dock.app`;
>   * Corrected `scripting_addition_perform_validation()` verification (`attrib = 0x7f`).
> * **Dedicated Thread for Mouse Event Tap** ([#2829](https://github.com/asmvik/yabai/issues/2829)):
>   * Runs `CGEventTap` on its own dedicated `pthread` with private `CFRunLoop`, completely eliminating delayed or dropped mouse clicks when heavy applications stall the Accessibility API on the main thread.
> * **Launchd Restart Loop Guard & Legacy Service Migration** ([#2808](https://github.com/asmvik/yabai/issues/2808)):
>   * Automatically unloads, disables, and deletes legacy `com.koekeishiya.yabai` launchd agent on service commands (`--install-service`, `--start-service`, `--restart-service`, `--stop-service`);
>   * Reports the PID of an already-running yabai process and exits cleanly with code `0` to prevent launchd runaway respawn loops and multi-gigabyte log files.
> * **Window Close BSP Gap Elimination & Auto-Healing** ([#2828](https://github.com/asmvik/yabai/issues/2828)):
>   * Full tree root flush on window un-tile so remaining windows automatically expand to fill freed space;
>   * Space validation on `WINDOW_FOCUSED` and `APPLICATION_FRONT_SWITCHED` cleans up zombie windows and restores proper tiling immediately upon closing windows (`Cmd+W`).
> * **Reliable BSP Retiling on Fullscreen Exit** ([#2758](https://github.com/asmvik/yabai/issues/2758), [#2576](https://github.com/asmvik/yabai/issues/2576), [#2653](https://github.com/asmvik/yabai/issues/2653), [#2716](https://github.com/asmvik/yabai/issues/2716)):
>   * Exiting native fullscreen or zoom-fullscreen now reliably re-integrates windows into the BSP layout;
>   * Fixes premature clearing of `WINDOW_MOVABLE` and debouncing false alarms during system transition animations.
> * **`external_bar` Consideration in Windowed Fullscreen** ([#2776](https://github.com/asmvik/yabai/issues/2776)):
>   * Toggling `windowed-fullscreen` mode now honors configured top and bottom padding for external status bars (e.g., Sketchybar).
> * **Per-App Focus-Follows-Mouse Exclusion Rule** ([#2750](https://github.com/asmvik/yabai/issues/2750)):
>   * Added `ffm=off` / `focus_follows_mouse=off` rule option to prevent focus from jumping to or from excluded apps (such as virtual machines, games, or graphic editors) when the mouse passes over them.
> * **Prevent Space Jump on Sticky & PiP Window Focus** ([#2825](https://github.com/asmvik/yabai/issues/2825)):
>   * When `skip_window_focus_animation` is enabled, focusing or clicking sticky windows (such as YouTube Picture-in-Picture players in Safari/Chrome or floating utility overlays) no longer erroneously triggers a space-switching gesture to the application's background space.
> * **Fix Frame Recalculation When Warping Sole Window Between Spaces** ([#2817](https://github.com/asmvik/yabai/issues/2817)):
>   * When dragging and dropping the only window on a space to warp into a window on another display or space (`mouse_drop_action warp`), the target window and newly moved window now properly capture their geometries and animate smoothly instead of freezing at their old bounds.
> * **Scratchpad Toggle Space Jumping Fix** ([#2807](https://github.com/asmvik/yabai/issues/2807)):
>   * Synchronized window space movement before focus change, preventing yabai from snapping back to the previous space when toggling a scratchpad window.

## About

<img align="right" width="40%" src="assets/screenshot.png" alt="Screenshot">

yabai is a window management utility that is designed to work as an extension to the built-in window manager of macOS.
yabai allows you to control your windows, spaces and displays freely using an intuitive command line interface and optionally set user-defined keyboard shortcuts using [&nearr;&nbsp;skhd][gh-skhd] and other third-party software.

The primary function of yabai is tiling window management; automatically modifying your window layout using a binary space partitioning algorithm to allow you to focus on the content of your windows without distractions.
Additional features of yabai include focus-follows-mouse, disabling animations for switching spaces, creating spaces past the limit of 16 spaces, and much more.

## Installation and Configuration

- The [&nearr;&nbsp;yabai&nbsp;wiki][yabai-wiki] has both brief and detailed installation instructions for multiple installation methods, and also explains how to uninstall yabai completely.
- Sample configuration files can be found in the [&nearr;&nbsp;examples][yabai-examples] directory. Refer to the [&nearr;&nbsp;documentation][yabai-docs] or the wiki for further information.
- Keyboard shortcuts can be defined with [&nearr;&nbsp;skhd][gh-skhd] or any other suitable software you may prefer.

## Requirements and Caveats

Please read the below requirements carefully.
Make sure you fulfil all of them before filing an issue.

|Requirement|Note|
|-:|:-|
|Operating&nbsp;System&nbsp;Intel x86-64|Big Sur 11.0.0+, Monterey 12.0.0+, Ventura 13.0.0+, Sonoma 14.0.0+, Sequoia 15.0+, Tahoe 26.0+, and Golden Gate 27.0+ is supported.|
|Operating&nbsp;System&nbsp;Apple Silicon|Monterey 12.0.0+, Ventura 13.0.0+, Sonoma 14.0.0+, Sequoia 15.0+, Tahoe 26.0+, and Golden Gate 27.0+ is supported.|
|Accessibility&nbsp;API|yabai must be given permission to utilize the Accessibility API and will request access upon launch. The application must be restarted after access has been granted.|
|Screen Recording|yabai must be given Screen Recording permission if and only if you want to enable window animations, and will request access when necessary. The application must be restarted after access has been granted.|
|System&nbsp;Preferences&nbsp;(macOS 11.x, 12.x)|In the Mission Control pane, the setting "Displays have separate Spaces" must be enabled.|
|System&nbsp;Settings&nbsp;(macOS 13.x, 14.x, 15.x, 26.x, 27.x)|In the Desktop & Dock tab, inside the Mission Control pane, the setting "Displays have separate Spaces" must be enabled.|

Please also take note of the following caveats.

|Caveat|Note|
|-:|:-|
|System&nbsp;Integrity&nbsp;Protection (Optional)|System Integrity Protection can be (partially) disabled for yabai to inject a scripting addition into Dock.app for controlling windows with functions that require elevated privileges. This enables control of the window server, which is the sole owner of all window connections, and enables additional features of yabai.|
|Code&nbsp;Signing|When building from source (or installing from HEAD), it is necessary to codesign the binary so it retains its accessibility and automation privileges when updated or rebuilt.|
|Finder&nbsp;Desktop|Some people disable the Finder Desktop window using an undocumented defaults write command. This breaks focusing of empty spaces and should be avoided when using yabai. To re-activate the Finder Desktop, run: "defaults write com.apple.finder CreateDesktop -bool true".|
|NSDocument-based&nbsp;Applications|Windows that utilize native macOS tabs such as Terminal and Finder, [do not behave correctly when creating tabs](https://github.com/asmvik/yabai/issues/68). Avoid creating tabs in these applications, consider alternatives that do not use NSDocument's tab system, or make these windows float using rules.|
|System&nbsp;Preferences&nbsp;(macOS 11.x, 12.x)|In the Mission Control pane, the setting "Automatically rearrange Spaces based on most recent use" should be disabled for commands that rely on the ordering of spaces to work reliably.|
|System&nbsp;Settings&nbsp;(macOS 13.x, 14.x, 15.x, 26.x)|In the Desktop & Dock tab, inside the Mission Control pane, the setting "Automatically rearrange Spaces based on most recent use" should be disabled for commands that rely on the ordering of spaces to work reliably.|
|System&nbsp;Settings&nbsp;(macOS 14.x, 15.x, 26.x)|In the Desktop & Dock tab, inside the Desktop & Stage Manager pane, the setting "Show Items On Desktop" should be enabled for display and space focus commands to work reliably in multi-display configurations.|
|System&nbsp;Settings&nbsp;(macOS 14.x, 15.x, 26.x)|In the Desktop & Dock tab, inside the Desktop & Stage Manager pane, the setting "Click wallpaper to reveal Desktop" should be set to "Only in Stage Manager" for display and space focus commands to work reliably.|

## License and Attribution

yabai is licensed under the [&nearr;&nbsp;MIT&nbsp;License][yabai-license], a short and simple permissive license with conditions only requiring preservation of copyright and license notices.
Licensed works, modifications, and larger works may be distributed under different terms and without source code.

Thanks to [@fools-mate][gh-fools-mate] for creating a logo and banner for this project and making them available for free.

Thanks to [@dominiklohmann][gh-dominiklohmann] for contributing great documentation, support, and more, for free.

## Disclaimer

Use at your own discretion.
I take no responsibility if anything should happen to your machine while trying to install, test or otherwise use this software in any form.
You acknowledge that you understand the potential risk that may come from disabling [&nearr;&nbsp;System&nbsp;Integrity&nbsp;Protection][external-about-sip] on your system, and I make no recommendation as to whether you should or should not disable System Integrity Protection.

<!-- Project internal links -->
[yabai-license]: LICENSE.txt
[yabai-examples]: https://github.com/asmvik/yabai/tree/master/examples
[yabai-wiki]: https://github.com/asmvik/yabai/wiki
[yabai-docs]: https://github.com/asmvik/yabai/blob/master/doc/yabai.asciidoc

<!-- Links to other GitHub projects/users -->
[gh-skhd]: https://github.com/asmvik/skhd
[gh-fools-mate]: https://github.com/fools-mate
[gh-dominiklohmann]: https://github.com/dominiklohmann

<!-- External links -->
[external-about-sip]: https://support.apple.com/en-us/HT204899
