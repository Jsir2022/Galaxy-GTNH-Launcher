# Galaxy GTNH Launcher

**English** · [简体中文](README.zh-CN.md)

Galaxy GTNH Launcher is an independent Qt 6 desktop launcher for the Galaxy community server. It prepares GregTech: New Horizons (GTNH) 2.9.0 in a player-selected directory and uses a bundled Java 25 runtime. The planned portable builds target Windows x64 and Linux x86-64.

**Development status:** Source preview. Minecraft Services AppID review and end-to-end online multiplayer testing are pending.

> **Not an official Minecraft product. Not approved by or associated with Mojang or Microsoft.**

## Installation and sign-in

1. Select an installation directory and a Minecraft download source.
2. Sign in on Microsoft's website using the OAuth 2.0 device code flow. The launcher checks the account's Minecraft: Java Edition entitlement and retrieves its player profile through Minecraft Services.
3. Only after these checks succeed, the launcher downloads and verifies the game and GTNH files. It then starts the client and connects to `gtnh.jsir.tech`.

Refresh credentials are stored locally through Windows Credential Manager or Linux Secret Service when available. If the system credential store is unavailable, sign in again on the next launch. [Minecraft EULA](https://www.minecraft.net/en-us/eula) · [Minecraft Usage Guidelines](https://www.minecraft.net/en-us/usage-guidelines)

## Download sources

- **Minecraft** uses Mojang endpoints by default. Players can opt into the [BMCLAPI community mirror](https://bmclapidoc.bangbang93.com/) for the client, game libraries and assets. Both options use the same pinned sizes and checksums. Authentication always uses Microsoft, Xbox Live and Minecraft Services endpoints.
- **GTNH** comes from the [GTNH project's download site](https://downloads.gtnewhorizons.com/Multi_mc_downloads/). The launcher installs the modpack locally. The source repository contains launcher code and installation metadata.

## Build

Requires CMake 3.22+, Qt 6.4+ (Widgets and Network), a C++20 compiler, and JDK 17+ for the extraction helper.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Configure an application with Minecraft Services access locally via `MSA_CLIENT_ID` or the `GTNH_MSA_CLIENT_ID` environment variable. Keep deployment identifiers and credentials out of Git. See [Build and packaging](docs/BUILD.md) for portable package inputs and development options.

## License and maintainer

Original launcher code is licensed under **GPL-3.0-or-later**. See [LICENSE](LICENSE) and the [third-party notices](THIRD-PARTY.md) for dependency licenses, metadata attribution and implementation references.

Maintained by [Jsir2022](https://github.com/Jsir2022).
