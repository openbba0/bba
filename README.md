# BBA

BBA（界面名 BlackBox）是一个 Android 应用分身沙箱。宿主把已安装应用再装进自己的虚拟系统里，用独立用户、独立数据目录和一套虚拟系统服务跑起来。同一包名可以开多份，每份用用户号区分。

宿主包名 `com.android.bba`，引擎包名 `com.android.bb`。产物文件名形如 `BlackBox_<版本>_<变体>.apk`。

## 能做什么

- 从本机已安装列表把应用装进沙箱，按用户号分身、启动、强停、清数据、卸载。
- 给分身创建桌面快捷方式。
- 在沙箱里维护自己的包管理、活动栈、任务、通知、账号、存储和定位。
- 可选接入本机已有的 Google 服务环境，供依赖 GMS 的分身使用。
- 设置里可以打开：隐藏 Root、守护进程、VPN 网络、允许截图（关闭 `FLAG_SECURE`）、小红书指纹落盘。
- 定位页可以给单个分身指定伪造坐标（预览功能）。
- 开发机可以通过 Spindle 连上宿主，把脚本送进指定分身进程。arm64 上内置 Frida Gum 的 Interceptor 和 Stalker。

## 工程模块

| 模块 | 角色 |
| --- | --- |
| `:app` | Kotlin 界面。应用列表、设置、GMS、伪造定位、关于页。入口是 `com.android.bba.app.App`。 |
| `:core` | 虚拟化引擎。Java/AIDL 系统服务、Binder 代理、IO 重定向，以及 `libblackbox.so`。 |
| `:beer` | SandHook 与 Xposed 兼容层，给访客进程里的 Java hook 用。 |
| `:reflection` | `BlackReflection` 运行时。用注解描述隐藏 API，运行时再反射调用。生成类在 `black.android.*`。 |
| `:compiler` | 对应的注解处理器，编译期生成反射代理。 |

版本号在根 `build.gradle` 里从 git 取：`versionCode` 是 `git rev-list --count HEAD`，`versionName` 是 `<count> - <git describe>`。

## 构建环境

| 项 | 值 |
| --- | --- |
| AGP | 8.13.2 |
| Kotlin | 1.9.23 |
| compileSdk | 35 |
| targetSdk | 28 |
| minSdk | 21 |
| JDK | app / core 为 21，beer 为 17 |
| NDK | 25.2.9519653 |
| ABI | `armeabi-v7a`、`arm64-v8a`，并打 universal APK |
| 页面大小 | 链接参数 `-z,max-page-size=16384`，适配 16 KB 页 |

```bash
./gradlew :app:assembleDebug
```

## 进程

`BlackBoxCore` 把进程分成三种：

- **Main**：宿主界面进程，进程名固定为 `com.android.bba`。装包、启动分身、设置、Spindle 监听都在这里。
- **Server**：虚拟系统进程。`BlackBoxSystem` 在这里拉起包管理、用户、活动、任务、存储、安装器、进程、账号、定位、通知这些服务。
- **BAppClient**：某一个分身。进程名会被改成被克隆应用的包名。`BActivityThread` 在访客 `Application` 创建前完成重定向、系统 hook，并注册到 Spindle。

界面通过 `BlackBoxCore.installPackageAsUser` / `launchApk` / `stopPackage` 操作某个 `userId` 下的分身。用户号来自应用列表的分页，0 是第一份。

## 引擎怎么把访客跑起来

访客并不装成系统里的独立应用。宿主把 APK 记进自己的包数据库，启动时用代理 Activity / Service / Provider 把真实组件接进来，再在客户端进程里换上访客的 `LoadedApk`、`Application` 和上下文。

Java 侧 `HookManager` 替换一批系统 Binder：活动、包、通知、定位、电话、网络、账号、存储、WebView，以及小米相关服务。调用落到宿主自己的服务，或被改写成当前分身的 uid、包名和路径。

Native 侧 `libblackbox.so`（`BoxCore`）在初始化时做这些事：

- 用 Dobby 做 inline hook，用 xdl 解析符号。
- `IO` 按规则把访客看到的路径重定向到沙箱目录（`NativeCore.addIORule` / `enableIO`）。
- Hook 文件系统、`UnixFileSystem`、`DexFile`、`VMClassLoader`、Binder、zlib。
- 绕过 hidden API 限制。
- `VirtualSpoof` 把一部分 `ro.*` 属性改成 Pixel 6 / Android 12 的值。

只在 **arm64** 编进两份静态库：

- Capstone，只开 AArch64 反汇编。
- Frida Gum 的一个子集：Interceptor（`replace` / `attach`）和 Stalker。`GumEngine` 会把 `libblackbox.so` 自己的可执行段排除出 Stalker。

对应 JNI 在 `NativeCore`：`gumReplace`、`gumRevert`、`gumStalkerFollow`、`gumStalkerUnfollow`、`gumStalkerExclude`、`gumStalkerEventCount`。armeabi-v7a 没有这层。

## 访客里的 Java hook

`HookEntry` 在访客 `Application.onCreate` 之前按包名分发。具体 hook 继承 `AppHook`：先准备 SandHook / `XposedCompat`，再执行子类的 `onHook`。子类在编译期登记进 `AppHookRegistry`。当前有 `RednoteHook`。`enabled()` 返回 false 时，这个包的访客进程不会装 hook。

## Spindle

Spindle 是开发机和进程之间的一条 TCP 通道，协议接近 Frida 命令行的用法。监听跑在虚拟系统进程 `com.android.bba:black` 里（`SpindleServer`）。

- 端口读 `persist.spindle.port`，缺省 **2407**，只绑环回地址。
- 主进程和访客通过 `ContentProvider.call("VM")` 拿到 `ISpindleService` 再注册。pid、uid 取自这次 Binder 调用。
- 开发机用 `tools/spindle` 连接。多台设备时要加 `-D <serial>`。USB 模式会 `adb forward` 到本机再连。

```bash
# 拉起一个已装进沙箱的包，注入脚本，再放行
python3 tools/spindle -U -f com.example.app -l tools/test_js_api.js

# 挂到已经在跑的分身
python3 tools/spindle -U -n com.example.app -l tools/test_native_hook.js

# 看已经注册的进程
python3 tools/spindle -U --list
```

帧是小端 4 字节长度加正文，单帧最大 1 MB。宿主侧 hello 以 `HOST` 开头，可以带 `target` 和 `spawn`。脚本正文直接下发；日志帧以 `LOG` 开头，RPC 帧以 `RPC` 开头。`spawn` 时客户端会在脚本发出后送 `UNLOCK`，访客在启动路径上等这帧，避免 `startActivity` 和注入互相卡住。

脚本到达目标进程后，`BoxCore::onSpindleScript` 调到 `NativeCore.onSpindleScript`。这里只把字节交给已注册的 `ScriptHandler`。**当前仓库里没有挂上 JavaScript 解释器**：对本进程的 RPC，服务端直接回 `js runtime is not in bba`。`tools/test_*.js` 写的是目标 API（`Memory`、`Interceptor`、`Stalker`、`Java`、`rpc.exports`），解释器还不在这条链路里。

`NativeCore.launchForSpindle` 会先停掉 user 0 上的该包再启动，供 `spawn` 使用。

## 目录

```
app/                 宿主界面（Kotlin）
core/                虚拟化引擎
  src/main/java/com/android/bb/   BlackBoxCore、系统服务、Java hook、客户端
  src/main/java/black/            隐藏 API 的反射声明
  src/main/cpp/                   libblackbox.so
    hook/            Dobby hook、GumEngine、SpindleServer
    utils/           路径、属性伪装、反检测
    gum/             arm64 Gum 子集
    capstone/        arm64 反汇编
    dobby/           预编译静态库
    xdl/             linker / 符号
beer/                SandHook 与 Xposed 兼容层
reflection/          BlackReflection 运行时
compiler/            注解处理器
tools/               spindle 客户端和示例脚本
```

## 界面结构

`WelcomeActivity` 是启动页，`MainActivity` 是主页，底栏三项：

- **应用**：按用户号分页的分身网格。长按可以强停、清数据、卸载、建快捷方式。
- **设置**：GMS、隐藏 Root、守护进程、VPN、关闭 `FLAG_SECURE`、小红书指纹落盘、发送日志。
- **我**：版本名和 versionCode。

伪造定位在分身菜单里进入 `FakeManagerActivity`，地图用 osmdroid。
