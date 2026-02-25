# 1g4-panel

`1g4-panel` is the OneG4 desktop panel module implemented in Qt6/C++. It starts one or more panel windows, loads panel plugins, and provides panel configuration UI (`panel/main.cpp`, `panel/oneg4panelapplication.cpp`).

## Why it exists

This repository contains the panel component used by OneG4 sessions, including autostart integration (`autostart/1g4-panel.desktop.in`) and standalone CLI launch (`panel/man/1g4-panel.1`).

## Status

Unknown from repo scan for overall project stability.

TODO: document explicit release/stability policy here.

Current signal: volume plugin rewrite work is active (`TODO.md`, `plugin-volume/docs/backend-target-matrix.md`, `plugin-volume/docs/design-decisions.md`).

## Features

- Main panel executable `1g4-panel` with CLI options `-h`, `--help-all`, `-v`, `-c/--config/--configfile` (`panel/oneg4panelapplication.cpp`, `panel/man/1g4-panel.1`).
- Plugin host supports both static and module plugins; module install target defaults to `${CMAKE_INSTALL_FULL_LIBDIR}/1g4-panel` (`cmake/BuildPlugin.cmake`, `panel/plugin.cpp`).
- Built plugins in this repo: `taskbar`, `statusnotifier`, `volume`, `worldclock`, `spacer` (`plugin-*/CMakeLists.txt`, top-level `CMakeLists.txt`).
- Default panel/plugin config includes `taskbar`, `worldclock`, `volume`, `statusnotifier` (`panel/resources/panel.conf`, `panel/panelpluginsmodel.cpp`).
- WM backend abstraction with `xcb` backend module plus dummy fallback when backend loading fails (`panel/backends/xcb/`, `panel/backends/oneg4dummywmbackend.*`, `panel/oneg4panelapplication.cpp`).
- Volume plugin targets PipeWire + WirePlumber + BlueZ and includes WirePlumber policy file management (`CMakeLists.txt`, `plugin-volume/docs/backend-target-matrix.md`, `plugin-volume/wireplumberpolicy.cpp`).

## Build

Dependencies from build files and CI:

- CMake >= 3.18 (`CMakeLists.txt`)
- Qt6: DBus, Widgets, Xml, Test (`CMakeLists.txt`, `tests/CMakeLists.txt`)
- KF6 WindowSystem (`CMakeLists.txt`)
- pkg-config and audio stack dev packages: `libpipewire-0.3`, `wireplumber-0.5`, `bluez` (`CMakeLists.txt`)
- Fedora package examples used in CI (`.github/workflows/ci.yml`, `.github/workflows/cmake.yml`)

Fedora example:

```bash
sudo dnf -y install \
  git cmake ninja-build gcc-c++ pkgconf-pkg-config \
  qt6-qtbase-devel qt6-qttools-devel \
  kf6-kwindowsystem-devel \
  libX11-devel libqtxdg-devel libqtxdg libxcb-devel \
  bluez-libs-devel pipewire-devel wireplumber-devel \
  glib2-devel pulseaudio-libs-devel alsa-lib-devel
```

Configure and build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

Install:

```bash
cmake --install build
```

Selected CMake options:

- `-DONEG4_DEV_VOLUME_ONLY=ON` builds only compat + volume plugin path (`CMakeLists.txt`)
- `-DONEG4_VOLUME_DEV_VERBOSE_LOGGING=ON`
- `-DONEG4_VOLUME_DEV_TEST_BACKENDS=ON`
- `-DONEG4_VOLUME_ENABLE_WIREPLUMBER_POLICY=ON|OFF`
- `-DONEG4_VOLUME_ENABLE_BLUETOOTH_BATTERY=ON|OFF`
- In-source builds are rejected (`CMakeLists.txt`)

## Run / Usage

Show help:

```bash
build/panel/1g4-panel --help
```

Run with default configuration lookup:

```bash
build/panel/1g4-panel
```

Run with explicit configuration file:

```bash
build/panel/1g4-panel --config /path/to/panel.conf
```

Autostart desktop file is installed to `${ONEG4_ETC_XDG_DIR}/autostart` (`autostart/CMakeLists.txt`).

## Configuration

- Default panel config template is `panel/resources/panel.conf` and is installed to `${ONEG4_ETC_XDG_DIR}/oneg4` (`panel/CMakeLists.txt`).
- The runtime settings object is created as `OneG4::Settings("panel")`; comments document `~/.config/oneg4/panel.conf` as the normal user file unless `--config` is used (`panel/oneg4panelapplication.h`, `panel/oneg4panelapplication.cpp`).
- If the user config file is missing, `OneG4::Settings` attempts to copy from system config locations (`OneG4/Settings.cpp`).
- `ONEG4PANEL_PLUGIN_PATH` adds module search paths for plugins/backends (`panel/plugin.cpp`, `panel/oneg4panelapplication.cpp`).
- `ONEG4_PANEL_PLUGINS_DIR` adds plugin desktop metadata search paths (`panel/oneg4panel.cpp`).
- Volume policy file path (when policy integration is used): `~/.config/wireplumber/wireplumber.conf.d/60-1g4-panel-volume.conf` (`plugin-volume/wireplumberpolicy.cpp`).

## Architecture

- `OneG4/`: shared compatibility utilities (`Settings`, `PluginInfo`, etc.).
- `panel/`: main application, panel layout/hosting, config dialogs, man page.
- `panel/backends/`: WM backend interface + implementations (`xcb` module and dummy backend).
- `plugin-*/`: panel plugin implementations.
- `plugin-volume/1g4-mixer/`: static mixer library integrated into volume plugin build.
- `tests/`: Qt tests and CMake guard tests (mostly volume/plugin integration checks).

Startup flow:

- `panel/main.cpp` starts `OneG4PanelApplication`.
- `OneG4PanelApplication` parses CLI, loads settings, loads WM backend, and creates panel instances.
- Panels discover plugin desktop files, instantiate plugins, and apply persisted settings.

## Testing

Run tests with CTest:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

List registered tests:

```bash
ctest --test-dir build -N
```

`tests/CMakeLists.txt` defines the test targets and guard checks. On the default build tree, `ctest -N` reports 26 tests.

CI workflows:

- `.github/workflows/cmake.yml`: Fedora Qt6 build
- `.github/workflows/ci.yml`: sanitizer matrix (`asan`, `ubsan`, `tsan`) plus CTest

## Troubleshooting

- Missing `libpipewire-0.3`, `wireplumber-0.5`, or `bluez` development files causes CMake configuration failure (`CMakeLists.txt`).
- If no WM backend module loads, the app falls back to the dummy backend and logs a warning (`panel/oneg4panelapplication.cpp`).
- In-source builds are blocked (`CMakeLists.txt`).

## License

GNU Lesser General Public License v2.1 (`LICENSE`).

## Contributing

Repo-specific expectations visible in this tree:

- Build out-of-tree (`cmake -S . -B build`).
- Run `ctest --test-dir build --output-on-failure` before sending changes.
- Follow `.clang-format` (Chromium base, `ColumnLimit: 120`, includes preserved).
- Keep `plugin-volume/docs/` and `tests/check_*` guard expectations aligned when changing volume plugin architecture/dependency behavior.

<div align="center">
  <sub>Built with ❤️ by the 1g4 team — <a href="https://1g4.org">1g4.org</a></sub>
</div>
