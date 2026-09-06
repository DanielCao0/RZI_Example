# Zephyr Getting Started 逐步详解

对照官方文档：
[Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)

本文按官方教程顺序说明**每一步在干什么、产出什么、和后面哪一步有关**。
默认以 **Ubuntu / Linux（含 WSL）** 为例；macOS / Windows 思路相同，只是包管理器和路径写法不同。

---

## 整体在搭什么

官方入门不是装一个 IDE，而是搭一条命令行工具链：

```text
系统依赖 (CMake/Ninja/Python/dtc…)
        ↓
Python venv + west
        ↓
west workspace（Zephyr 源码 + modules）
        ↓
Python 依赖 + CMake 登记（zephyr-export）
        ↓
Zephyr SDK（交叉编译器 + QEMU/OpenOCD…）
        ↓
west build / west flash
```

记住三样东西即可：

| 东西 | 是什么 | 官方默认位置 |
|------|--------|----------------|
| **Workspace** | 源码工作区（Zephyr + 模块） | `~/zephyrproject` |
| **venv** | Python / west 的隔离环境 | `~/zephyrproject/.venv` |
| **SDK** | 交叉编译工具链（不是源码） | `~/zephyr-sdk-<版本>` |

---

## 第一步：Select and Update OS（更新系统）

**在干什么**
先把系统软件源/补丁更新到较新状态，减少后面装依赖时版本过旧、缺库等问题。

**Ubuntu 示例**

```bash
sudo apt update
sudo apt upgrade
```

**产出**
系统包索引和已装软件较新；这一步本身不安装 Zephyr。

**注意**
官方 Ubuntu 覆盖 **24.04 LTS 及更新**；其它发行版看 [Install Linux Host Dependencies](https://docs.zephyrproject.org/latest/develop/getting_started/installation_linux.html)。

---

## 第二步：Install dependencies（装主机依赖）

**在干什么**
安装**宿主机**上编译 Zephyr 需要的工具。这些是「在你电脑上跑的程序」，不是给板子跑的交叉编译器。

| 工具 | 作用 |
|------|------|
| **CMake** | 生成构建系统（配置阶段） |
| **Ninja / Make** | 真正执行编译 |
| **Python 3** | west、构建脚本、部分工具 |
| **dtc** | 设备树编译 |
| **git / wget** | 拉代码、下 SDK |
| **gperf / ccache / dfu-util…** | 构建加速、烧录等辅助 |
| **gcc / g++-multilib…** | 部分主机侧/模拟相关需求 |
| **libsdl2-dev** | 某些 native/显示样例 |

**官方最低版本（文档表格）**

| 工具 | 最低版本 |
|------|----------|
| CMake | 3.28.0 |
| Python | 3.12（强烈建议；更新版本有时反而会踩坑） |
| dtc | 1.4.6 |

**Ubuntu 示例**

```bash
sudo apt install --no-install-recommends git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget python3-dev python3-venv python3-tk \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
```

装完建议核对：

```bash
cmake --version
python3 --version
dtc --version
```

**产出**
系统里有了能跑 `west build` 所需的主机工具（`/usr/bin/cmake` 等）。

**和 SDK 的区别**
- 这一步：主机工具（CMake、Python…）
- 后面 `west sdk install`：ARM/RISC-V 等**交叉**工具链

---

## 第三步：Get Zephyr and install Python dependencies

这一大步包含多小步，是「把 Zephyr 源码世界立起来」。

### 3.1 创建 Python 虚拟环境

**在干什么**
为 Zephyr 单独建一个 Python 环境，避免和系统/其它项目的 pip 包互相污染。

```bash
python3 -m venv ~/zephyrproject/.venv
```

**产出**
目录 `~/zephyrproject/.venv/`（此时还几乎是空壳 Python）。

**习惯**
官方把 venv 放在 workspace 目录下，方便「一个工程一套 Python」。

---

### 3.2 激活虚拟环境

```bash
source ~/zephyrproject/.venv/bin/activate
```

**在干什么**
让当前 shell 优先使用 `.venv` 里的 `python` / `pip`。提示符前通常会出现 `(.venv)`。

**重要**
**每个新开的终端都要重新 activate**，否则可能：

- 找不到 `west`
- 用到系统里另一套 west/包，报错很诡异

退出：`deactivate`。

---

### 3.3 安装 west

```bash
pip install west
```

**在干什么**
安装 Zephyr 的元工具 **west**：多仓管理 + 构建/烧录等扩展命令的入口。

**产出**
`.venv` 里有 `west` 命令（例如 `~/zephyrproject/.venv/bin/west`）。

**west 是什么（一句话）**
类似「带 Zephyr 插件的 git 多仓库管家」：`west update` 拉模块，`west build` / `west flash` 编、烧。

---

### 3.4 获取 Zephyr 源码（west init + west update）

```bash
west init -m https://github.com/zephyrproject-rtos/zephyr ~/zephyrproject
cd ~/zephyrproject
west update
```

**`west init` 在干什么**

- 把 `~/zephyrproject` 建成一个 **west workspace**
- 创建 `.west/`（里面有 config，记录 manifest 在哪）
- 按 `-m` 指定的仓库，把 **Zephyr 主仓** clone 进来（通常是 `zephyr/`）

**`west update` 在干什么**

- 读取 Zephyr 的 `west.yml`（manifest）
- 把列出的 **modules**（HAL、库、bootloader 相关组件等）全部拉下来
  例如常见结构：`modules/`、`bootloader/`、`tools/` 等

**产出（典型）**

```text
~/zephyrproject/
├── .west/          # workspace 元数据（如何找到 zephyr）
├── .venv/          # Python 环境（你前面建的）
├── zephyr/         # Zephyr 内核与官方源码树
├── modules/        # 各类外部模块
├── bootloader/     # 如有
└── ...
```

**关键点：工程如何「找到 Zephyr」**
之后在这个目录里跑 `west build` 时，west 会：

1. 向上找到 `.west/`
2. 读 `.west/config` 里的 `[zephyr] base = ...`
3. 定下 `ZEPHYR_BASE`（官方布局下即 `~/zephyrproject/zephyr`）

也就是说：**源码位置由 west workspace 绑定，不是靠猜家目录。**

**Tip（官方）**
若磁盘紧、不需要全量 vendor HAL，可在 `west update` 前配置 [Project Groups](https://docs.zephyrproject.org/latest/develop/west/workspaces.html)，减少下载量。

---

### 3.5 安装 Zephyr 的 Python 依赖

```bash
west packages pip --install
```

**在干什么**
根据**当前这份** Zephyr（及 modules）声明的 Python 需求，往当前 venv 里装包。

**为什么不手写 pip install -r**
不同 Zephyr 版本、不同模块要求不同；`west packages` 从 workspace 里读，保证和源码版本匹配。

**注意（官方）**
装这些依赖时，**有可能升级或降级 west 本身**，一般正常。

**产出**
venv 里具备构建脚本、西式工具链辅助等所需的 Python 包。

---

### 3.6 Export：`west zephyr-export`

```bash
west zephyr-export
```

**在干什么**
把**当前这份 Zephyr 源码**登记到 **CMake user package registry**，让：

```cmake
find_package(Zephyr ...)
```

在没有手动设 `ZEPHYR_BASE` 时也能找到这份安装。

**Linux 上实际写入**

```text
~/.cmake/packages/Zephyr/<md5>
# 文件内容类似：
# /home/你/zephyrproject/zephyr/share/zephyr-package/cmake
```

同时也会登记 `ZephyrUnittest`。

**它不找 SDK**
`zephyr-export` **只登记 Zephyr 源码包**，和交叉编译器无关。

**和日常 `west build` 的关系**

- 在 workspace 里用 `west build`：主要靠 `.west/config` 定 `ZEPHYR_BASE`
- `zephyr-export`：给纯 CMake / `find_package(Zephyr)` 场景补一条发现路径

官方教程两套都做，是为了后面各种构建方式都能找到同一份 Zephyr。

**CMake registry 是什么（一句话）**
CMake 的「通讯录」：记下包名对应哪个目录，方便 `find_package`。

---

## 第四步：Install the Zephyr SDK

```bash
cd ~/zephyrproject/zephyr
west sdk install
```

**在干什么**

1. 读取当前 Zephyr 仓库里的 `SDK_VERSION`（需要哪版 SDK）
2. 下载对应的 Zephyr SDK 包
3. 解压安装（默认到家目录下的 `~/zephyr-sdk-<版本>`）
4. 运行 setup，安装所选架构工具链，并登记 **Zephyr-sdk** 到 CMake registry

**SDK 里面是什么**

- **不是** Zephyr 源码
- **是** 各架构交叉编译器（GCC/binutils/GDB 等）
- 以及主机侧工具：定制 **QEMU**、**OpenOCD** 等（仿真、烧录、调试）

**默认安装位置**
未指定路径时，`west sdk install` 通常装到：

```text
~/zephyr-sdk-<版本号>
# 例如 ~/zephyr-sdk-1.0.1
```

可用选项改安装目录、只装某些架构（见 `west sdk install --help`）。只装 ARM 可显著省空间，例如产品工程常用：

```bash
west sdk install -t arm-zephyr-eabi
```

**编译时如何找到 SDK（和 install 的区别）**

| 时机 | 谁在做 | 含义 |
|------|--------|------|
| `west sdk install` | west | **下载并安装** SDK（已存在同版本可能复用） |
| `west build` | Zephyr 的 CMake | **查找并使用**已安装的 SDK |

查找顺序大致包括：

1. 环境变量 `ZEPHYR_SDK_INSTALL_DIR`
2. `~/.cmake/packages/Zephyr-sdk`（setup 写过）
3. 官方默认搜索路径：`$HOME`、`$HOME/.local`、`$HOME/.local/opt`、`/opt`、`/usr/local` 等

**手动安装**
若不用 `west sdk`，见官方 [Zephyr SDK installation](https://docs.zephyrproject.org/latest/develop/toolchains/zephyr_sdk.html)。

---

## 第五步：Build the Blinky Sample（编译样例）

```bash
cd ~/zephyrproject/zephyr
west build -p always -b <your-board-name> samples/basic/blinky
```

**在干什么**

1. west 确认当前 workspace / `ZEPHYR_BASE`
2. 调用 CMake，配置「板子 + 应用」
3. CMake 找到 Zephyr，再找到 SDK 工具链
4. Ninja/Make 编译出固件（elf/hex/bin 等）

**参数含义**

| 参数 | 含义 |
|------|------|
| `-b <board>` | 目标板（可用 `west boards` 查名字） |
| `samples/basic/blinky` | 应用源码目录 |
| `-p always` | 强制干净配置/构建，避免入门时脏缓存；熟练后可用 `-p auto` |

**多核板注意**
有的板要写 SoC/CPU 簇，例如：`nrf5340dk/nrf5340/cpuapp`。

**产出**
默认在应用相关的 `build/` 目录下生成固件产物（如 `zephyr/zephyr.elf` 等）。

**板不支持 Blinky 时**
可改试 Hello World 等其它 sample。

---

## 第六步：Flash the Sample（烧录）

```bash
west flash
```

**在干什么**
把刚编好的固件通过调试器/串口下载器等写到板子上。具体后端依赖板级配置（OpenOCD、J-Link、pyocd、dfu-util…）。

**常见额外要求**

- 板子 USB 已连接、供电正常
- 主机装了该板需要的烧录工具
- Linux 上可能要配 **udev rules**，普通用户才能访问调试器（见官方 Setting udev rules）

**产出**
板子上跑起样例（Blinky 则 LED 闪烁）。

---

## 官方步骤与「谁找谁」关系图

```text
┌─────────────────────────────────────────────────────────┐
│  ~/zephyrproject/.venv                                  │
│  pip 安装的 west、Python 包                             │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  west workspace: ~/zephyrproject                        │
│  .west/config  →  ZEPHYR_BASE = .../zephyr              │
│  west update   →  modules / HAL 等                      │
└─────────────────────────────────────────────────────────┘
                          │
          ┌───────────────┴───────────────┐
          ▼                               ▼
 west zephyr-export              west sdk install
 登记 Zephyr 源码                 下载/安装工具链
 ~/.cmake/packages/Zephyr         ~/zephyr-sdk-*
                                  ~/.cmake/packages/Zephyr-sdk
          │                               │
          └───────────────┬───────────────┘
                          ▼
                 west build / west flash
              CMake 拼起源码 + SDK 再编译烧录
```

---

## 每次新开终端的最小动作

官方环境装好后，日常通常只需：

```bash
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject
# 然后 west build / west flash ...
```

一般**不必**每次再跑：

- `west init` / `west update`（除非要更新源码）
- `west zephyr-export`（除非换了 Zephyr 路径或清过 registry）
- `west sdk install`（除非换 SDK 版本或重装工具链）

---

## 和「产品工程」（如 RAK9156）的对应关系

官方教程是「干净的上游入门布局」。产品工程往往是同一套机制，只是目录名不同：

| 官方入门 | 典型产品工程 |
|----------|----------------|
| `~/zephyrproject` | 例如 `~/RAK9156` |
| manifest 常是 `zephyr` | 可能是应用仓（如 `bms`）+ `west.yml` |
| 全量或默认 SDK | 常只装 `arm-zephyr-eabi` |
| `~/zephyr-sdk-1.0.1` | 可能是 `~/zephyr-sdk`（0.17.4 等） |

机制不变：**west 管源码 workspace，SDK 管交叉编译器，CMake registry 是可选的发现加速/备用路径。**

---

## 官方文档入口

- 入门总览：https://docs.zephyrproject.org/latest/develop/getting_started/index.html
- Linux 依赖补充：https://docs.zephyrproject.org/latest/develop/getting_started/installation_linux.html
- SDK 说明：https://docs.zephyrproject.org/latest/develop/toolchains/zephyr_sdk.html
- West：https://docs.zephyrproject.org/latest/develop/west/index.html

---

## 一句话总结每步

1. **更新系统** — 减少依赖版本坑
2. **装主机依赖** — CMake/Python/dtc 等「电脑上的工具」
3. **venv + west** — 隔离的 Python 与元工具
4. **west init/update** — 拉齐 Zephyr 源码和 modules
5. **west packages** — 按当前源码装 Python 依赖
6. **west zephyr-export** — 让 CMake `find_package(Zephyr)` 找得到源码
7. **west sdk install** — 下载安装交叉编译工具链
8. **west build** — 用源码 + SDK 编出固件
9. **west flash** — 把固件写到板子

其中最容易混淆的一点：

> **`west sdk install` = 把 SDK 装到本机**
> **`west build` 时的 CMake = 再去找到并使用这份 SDK**
> **`west zephyr-export` = 只登记 Zephyr 源码，不负责 SDK**
