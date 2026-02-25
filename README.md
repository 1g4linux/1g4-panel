# 1g4-panel

A modern desktop panel for OneG4.

1g4-panel gives you the core panel features you expect (taskbar, tray, clock, volume, spacing/layout tools) in a fast, lightweight Qt6/KF6 panel designed for everyday use.

## What it does

1g4-panel provides:

- **Taskbar** for open windows
- **System tray** (status icons)
- **Volume control** with integrated audio controls
- **World clock**
- **Spacer** plugin for layout customization

The volume plugin is being actively improved to provide a better built-in audio experience, including more reliable Bluetooth headset behavior.

## Current status

1g4-panel is actively developed and usable, with ongoing work focused on polishing the built-in volume/audio experience.

## Getting started

After installing 1g4-panel from your distro/package source, you can:

- Launch it from your desktop/session menu, or
- Start it as part of your session (autostart)

If your system uses OneG4 defaults, the panel should start with a ready-to-use layout.

## First run and configuration

On first run, 1g4-panel uses a default panel layout and plugins.

You can customize things like:

- Panel position and size
- Plugin order
- Clock display
- Volume behavior
- Tray/taskbar layout

If you already have a panel configuration, 1g4-panel will use it automatically.

## Included plugins

This repository includes these plugins:

- Taskbar
- Status Notifier (system tray)
- Volume
- World Clock
- Spacer

A typical default layout includes:

- Taskbar
- World Clock
- Volume
- Status Notifier

## Audio and Bluetooth

The built-in volume plugin is intended to reduce the need for a separate audio control app for common tasks.

It is designed to improve:

- Output/input switching
- Device selection
- Bluetooth headset handling
- Day-to-day volume/mute controls

Behavior may vary depending on your system audio setup.

## License

GNU Lesser General Public License v2.1 (`LICENSE`)

<div align="center">
  <sub>Built with ❤️ by the 1g4 team — <a href="https://1g4.org">1g4.org</a></sub>
</div>
