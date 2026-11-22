# Build and packaging

Build the launcher with CMake 3.22+, Qt 6.4+ (Widgets and Network), C++20, and JDK 17+. The shipped JRE is a separate runtime for the game and archive helper.

- `MSA_CLIENT_ID` sets the public Microsoft application ID at build time. The app registration needs personal Microsoft accounts, public-client device flow and Minecraft Services access. `GTNH_MSA_CLIENT_ID` overrides that setting for local runs.
- `GTNH_SERVER` and `GTNH_SERVER_PORT` set the initial server address. The default is `gtnh.jsir.tech` with the default Minecraft port and normal SRV resolution.
- `GTNH_ENABLE_INSTALL_DIAGNOSTICS=ON` builds a development-only install probe. Keep it off for player builds.

## Assemble a portable test build

Pass the build output directory, Qt libraries (DLL directory on Windows), Qt plug-in directory, pinned Temurin 25.0.5.1+2 x64 JRE archive, and a directory containing the two Galaxy client add-ons. The packaging tool requires Python 3.12+ and verifies the JRE and mod hashes.

```text
python3 tools/package.py linux    build-linux   QT_LIB QT_PLUGINS LINUX_JRE_ARCHIVE dist/GTNH-Linux --server-mods MODS_DIR
python3 tools/package.py windows build-windows QT_BIN QT_PLUGINS WINDOWS_JRE_ZIP  dist/GTNH-Windows --server-mods MODS_DIR
```

Start the Linux package with `启动 GTNH.sh` and the Windows package with `GtnhLauncher.exe`. Linux requires compatible system libraries and a working graphics environment. The launcher installs into `GTNH-Launcher-2.9.0` under the selected folder and writes `game-console.log` there.

For a public binary release, assemble the notices and corresponding-source materials for every bundled dependency. See [Third-party notices](../THIRD-PARTY.md).
