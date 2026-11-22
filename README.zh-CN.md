# Galaxy GTNH Launcher

[English](README.md) · **简体中文**

Galaxy GTNH Launcher 是面向 Galaxy 社区服务器的独立 Qt 6 桌面启动器。它在玩家选择的目录安装格雷科技：新视野（GTNH）2.9.0，使用内置 Java 25 启动。便携包面向 Windows x64 和 Linux x86-64。

**开发状态**：源码预览。目前等待 Minecraft Services AppID 审核，实际联网进服仍待验收。

> **非 Minecraft 官方产品。未经 Mojang 或 Microsoft 批准，与其无关联。**

## 安装与登录

1. 选择安装目录和 Minecraft 下载源。
2. 在微软网页通过 OAuth 2.0 设备码流程登录。启动器通过 Minecraft Services 验证 Minecraft Java 版权益并获取玩家资料。
3. 验证通过后，启动器下载并校验游戏和 GTNH 文件，再启动客户端连接 `gtnh.jsir.tech`。

系统提供 Windows 凭据管理器或 Linux Secret Service 时，刷新凭据保存在本机。系统凭据库不可用时，下次启动需要重新授权。[Minecraft EULA](https://www.minecraft.net/en-us/eula) · [Minecraft 使用指南](https://www.minecraft.net/zh-hans/usage-guidelines)

## 下载源

- **Minecraft** 默认使用 Mojang 官方源。玩家可选择 [BMCLAPI 国内社区镜像](https://bmclapidoc.bangbang93.com/)下载客户端、游戏库和资源。两条线路使用相同的固定文件大小和摘要。登录请求始终走微软、Xbox Live 和 Minecraft Services。
- **GTNH** 从 [GTNH 项目下载站](https://downloads.gtnewhorizons.com/Multi_mc_downloads/)获取，在玩家本机安装。源码仓库提供启动器代码和安装元数据。

## 构建

需要 CMake 3.22+、Qt 6.4+（Widgets 和 Network）、C++20 编译器，以及编译解压助手用的 JDK 17+。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

默认使用本项目的公开 AppID。自行构建可通过 `MSA_CLIENT_ID` 配置已获 Minecraft Services 接入的应用。便携包所需文件和开发选项见[构建与打包指南](docs/BUILD.md)。

## 许可与维护者

启动器原创代码采用 **GPL-3.0-or-later**。依赖许可证、元数据来源和参考项目见 [许可证](LICENSE) 与 [第三方声明](THIRD-PARTY.md)。

维护者为 [Jsir2022](https://github.com/Jsir2022)。
