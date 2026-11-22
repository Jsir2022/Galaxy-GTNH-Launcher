# 第三方来源

- Qt 6 以动态库分发。当前代码使用 Core、GUI、Widgets 和 Network。发行时以实际构建的模块、插件和打包依赖为准，提供对应的第三方许可文本、对应源码或符合条款的源码获取方式，并支持替换 Qt 库。参见 [Qt 官方许可义务](https://www.qt.io/development/open-source-lgpl-obligations)。
- Java 25 使用 [Eclipse Temurin 25.0.5.1+2 x64 JRE](https://github.com/adoptium/temurin25-binaries/releases/tag/jdk-25.0.5.1%2B2)，保留运行时附带的 legal 目录。根据实际二进制确认发行所需的对应源码。
- 固定 GTNH 2.9.0 元数据的数据源是 [GTNH 官方客户端档案](https://downloads.gtnewhorizons.com/Multi_mc_downloads/GT_New_Horizons_2.9.0_Java_17-26.zip)。生成工具检查输入档案 SHA-256，提取启动所需的数据。客户端档案、模组、Minecraft 文件及资源在玩家首次安装时从原来源获取。
- Galaxy 测试包附加 Chain Engineering 0.1.0-alpha.7 与 Qz-UILib 1.8.2，两个 JAR 与正式服及本地对应客户端逐字节一致。附加 JAR 作为构建输入存放于 Git 外，公开发行前确认其再分发条件及对应源码。
- [PCL2](https://github.com/Hex-Dragon/PCL2)、[Prism Launcher](https://github.com/PrismLauncher/PrismLauncher)、[packwiz](https://github.com/packwiz/packwiz) 和梦鱼用于设计评估。本项目的 C++ 启动逻辑、登录和 Java 解压代码独立实现。微软设备码请求字段参照 [微软设备授权协议](https://learn.microsoft.com/en-us/entra/identity-platform/v2-oauth2-device-code) 与 Prism 的微软、Xbox、Minecraft 服务调用流程。
