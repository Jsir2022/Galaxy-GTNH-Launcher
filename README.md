# Galaxy GTNH Launcher

Qt 6 启动器首版，面向 GT New Horizons 2.9.0 和 Java 25。发行包内置 Java。玩家打开启动器，选一个安装目录，在微软网页完成授权。启动器验证 Minecraft Java 版权益后再下载 Minecraft 和 GTNH，安装完成后启动客户端并连接指定服务器。

Not an official Minecraft product. Not approved by or associated with Mojang or Microsoft.

本项目源码采用 GPL-3.0-or-later。第三方组件分别适用其许可，见 [来源清单](THIRD-PARTY.md)。

## 构建

依赖 CMake 3.22+、Qt 6.4+（Widgets、Network）、C++20 编译器和构建用 JDK 17+。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMSA_CLIENT_ID=项目公开应用ID
cmake --build build --config Release
```

发行用 ID 需要支持个人微软账号、公开客户端设备码授权和 Minecraft Services API。开发者可用 `GTNH_MSA_CLIENT_ID` 环境变量覆盖构建设置。首发目标默认为 `gtnh.jsir.tech`，使用 Minecraft 默认端口及其 SRV 解析。构建时可用 `GTNH_SERVER`、`GTNH_SERVER_PORT` 指向其他正版验证测试服。

便携包由同目录中的程序、`Extract.class`、Qt 运行库与插件、`runtime` 组成。发行包组装工具使用 Python 3.12+。传入构建产物目录、Qt 库（Windows 为 DLL 目录）、Qt 插件目录和上游 Temurin 25.0.5.1+2 JRE 档案。使用 `--server-mods` 提供 Galaxy 客户端的两个固定附加模组，工具会校验其 SHA-256 并放入发行包。

```text
python3 tools/package.py linux    build-linux   QT_LIB QT_PLUGINS LINUX_JRE_ARCHIVE dist/GTNH-Linux --server-mods MODS_DIR
python3 tools/package.py windows build-windows QT_BIN QT_PLUGINS WINDOWS_JRE_ZIP  dist/GTNH-Windows --server-mods MODS_DIR
```

Linux 包由「启动 GTNH.sh」启动，依赖目标发行版提供兼容的系统库及图形驱动。Linux 使用桌面 Secret Service 保存授权刷新凭据，Windows 使用凭据管理器。桌面没有可用凭据库时，每次打开需重新授权。Windows 包通过 `GtnhLauncher.exe` 启动。测试包用于内部验收，公开发布需补齐所分发运行库的对应源码及许可材料。

## 当前范围

- 固定安装官方 GTNH 2.9.0 Java 17–26 客户端档案及 Galaxy 所需的附加模组，并始终使用内置 Java 25 启动。
- 游戏文件保存于所选目录下的 `GTNH-Launcher-2.9.0`。游戏控制台输出保存于其下的 `game-console.log`。
- 开发者可用 `-DGTNH_ENABLE_INSTALL_DIAGNOSTICS=ON` 构建专用安装探针。发行构建默认关闭探针，玩家须先完成授权。
- 自更新、整合包增量同步与服主发行工具留待下一阶段。
