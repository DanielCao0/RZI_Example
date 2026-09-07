# 用 Docker 搭建 Zephyr 开发环境（本机实际步骤）

日期：2026-08-14（WSL2 + Docker Desktop）
工程目录：`/home/daniel/rzi`

本文按**实际做过的顺序**写：每一步干什么、为什么、产物在哪。日常命令在文末。

对照：

- 本机入门概念：[zephyr-getting-started-explained.md](./zephyr-getting-started-explained.md)
- USP / LBM 分层：[doc/README.md](./doc/README.md)

---

## 做成之后长什么样

```text
/home/daniel/rzi-workspace/                 ← west 工作区根 = git 仓库根
├── docker/Dockerfile             # 镜像：west + SDK 1.0.1
├── scripts/container.sh              # 进容器 / init / 编译
├── app/                          # 你的应用 + 清单（老板）
│   ├── west.yml                  # 锁 Zephyr 提交，并加入 usp_zephyr / usp
│   ├── CMakeLists.txt
│   ├── prj.conf
│   ├── src/main.c
│   └── boards/rak4631_nrf52840.overlay
├── .west/config                  # west init -l app 生成，gitignore
├── zephyr/                       # 当前树 4.4.99（从旧 zephyrproject 挪上来）
├── modules/                      # Zephyr HAL 等 + modules/lib/usp
├── usp_zephyr/                   # Semtech Zephyr 封装（west 拉 main）
├── bootloader/  tools/
└── （镜像）rzi-zephyr:latest
```

| 内容 | 位置 |
|------|------|
| west、SDK | 镜像 `rzi-zephyr:latest`（`/opt/zephyr-sdk`） |
| 清单 | 本机 `west.yml` |
| `.west` | 本机仓库根（指向 `west.yml`） |
| Zephyr 源码 | 本机 `zephyr/`，容器里 `/workdir/zephyr` |
| 你的应用 | 本机 `app/`，容器里 `/workdir/app` |
| USP 例程 | `usp_zephyr/samples/usp/` |

容器挂载：**整个 `rzi` → `/workdir`**，工作目录 `/workdir`，`ZEPHYR_BASE=/workdir/zephyr`。

---

## 第 0 步：先有能编过的树（Zephyr 当老板）

更早已经做过：

1. `docker/Dockerfile` 做出镜像 `rzi-zephyr:latest`（Ubuntu 24.04 + SDK **1.0.1**）。
2. 源码在 `rzi/zephyrproject/`，`.west` 听 `zephyr/west.yml`（Zephyr 当老板）。
3. `app/` 是树外应用；容器当时把 `zephyrproject` 挂到 `/workdir`，把 `app` 另挂到 `/app`。
4. `./scripts/container.sh build` 已能编 `rak4631/nrf52840`（Zephyr **4.4.99**）。

当时 Zephyr 提交（后来写进 `west.yml` 的 `revision`）：

```text
161f758ba363ec90cd9b727a5d82e6c86efa85ad
# git describe: v4.4.0-11560-g161f758ba36
```

`app/` 当老板**不是**再下一份 Zephyr，而是换「谁的 `west.yml` 说了算」，并复用这棵树。

---

## 第 1 步：写 `west.yml`（你是老板）

文件：[west.yml](./west.yml)

要点：

| 写法 | 结果 |
|------|------|
| `self.path: app` | 工作区里应用在 `app/` |
| `zephyr` + `import: true` | 沿用 Zephyr 自己的模块清单（HAL、mcuboot…） |
| `zephyr.revision` = 上面那串 SHA | **钉死当前 4.4.99**，不会去拉 v4.2.0 |
| `usp_zephyr` / `usp`，`revision: main` | 跟 Semtech 当下分支 |
| **不要** `usp_zephyr: import: true` | 官方 `usp_zephyr/west.yml` 里写死了 `zephyr: v4.2.0`，一 import 就会换树 |

官方 `usp_zephyr`（main）自己的清单仍是：

```yaml
- name: zephyr
  revision: v4.2.0
  import: true
- name: usp
  path: modules/lib/usp
  revision: main
  submodules: true
```

我们只抄 **usp 这一条**，Zephyr 版本由我们自己锁。

`usp` 必须单独列：`usp_zephyr` 是 Zephyr 侧 HAL/CMake/盾；协议栈在 `Lora-net/usp`（LBM + RAC），west 路径约定是 `modules/lib/usp`。

---

## 第 2 步：把已有源码挪到工作区根

west 的 `import: true` 会按 **工作区根** 解析模块路径（`modules/hal/nordic` 等），不是按 `zephyr/` 子目录。所以不能把工作区仍放在 `zephyrproject/`、却让清单在仓库根的 `app/`——模块会对不齐。

同一文件系统上 `mv` 即可（不重新从 GitHub 拉 9G+）：

```bash
cd /home/daniel/rzi
mv zephyrproject/zephyr .
mv zephyrproject/modules .
mv zephyrproject/bootloader .
mv zephyrproject/tools .
rm -rf zephyrproject/.west    # 旧老板的元数据，不能留
```

旧目录 `zephyrproject/` 里只剩无关文件（如 `doc/`），可忽略或以后删。

`.gitignore` 忽略 west 拉下来的树和构建产物：`.west/`、`zephyr/`、`modules/`、`usp_zephyr/`、`bootloader/`、`tools/`、`build/app/`、`build-*/`、`zephyrproject/`。

---

## 第 3 步：改 Docker 挂载（整仓 → `/workdir`）

[scripts/container.sh](./scripts/container.sh) 相对「Zephyr 当老板」时改了这些：

1. `WS` 从 `rzi/zephyrproject` 改为 **`rzi`（仓库根）**。
2. 只挂 `-v rzi:/workdir`，**不再**另挂 `/app`。
3. 默认工作目录 `/workdir`（不是 `/workdir/zephyr`）。
4. 默认编译：

   ```bash
   west build -p always -b rak4631/nrf52840 -d /workdir/build/app /workdir/app
   ```

5. 容器里给 Git 设 `http.version=HTTP/1.1`（用环境变量 `GIT_CONFIG_*`，**不要** `git config --global`），减轻 Clash TUN 下 HTTP/2 `CANCEL` / GnuTLS 断流。
6. `--user $(id -u):$(id -g)`（本机 UID **1001**），否则写不了挂载目录。

**不要**在容器里对 `/workdir` 做 `west init -l /workdir`：west 会把 `.west` 放到**父目录** `/`，权限失败。正确是在工作区根执行：

```bash
west init -l app
```

含义：清单仓库是本地 `app/`（里面已有 `west.yml`），在**当前目录**生成 `.west/`。

脚本里 `init` / 首次 `build` / `shell` 会自动跑：`west init -l app` → `west update` → `west zephyr-export`。已有 `.west/` 则跳过。

生成的 `.west/config`：

```ini
[manifest]
path = app
file = west.yml

[zephyr]
base = zephyr
```

---

## 第 4 步：`west update` 只补 USP

Zephyr 和 HAL 已在本地且 SHA 对得上，`west update` **不必**再拉整棵 Zephyr。真正要上网的是：

```bash
./scripts/container.sh shell -lc 'west update usp_zephyr usp'
```

本机结果：

- `usp_zephyr` → `main`（当时 `bfacd43`）
- `usp` → `modules/lib/usp`，`main`

可用 `west list zephyr usp_zephyr usp` 核对三列：路径、revision、url。

GitHub 不稳时多试几次即可；脚本对完整 `west update` 有最多 5 次重试。

---

## 第 5 步：4.4 与 usp_zephyr 板级 yaml 差一行

`usp_zephyr` 把 `board_root` 指到自己的 `boards/`。其 `boards/seeed/xiao_nrf54l15/board.yml` **没有 `full_name`**。Zephyr 4.4 的 `board-schema.yaml` 要求 `name` 与 `full_name` 成对，CMake 在找 `rak4631` 时就会扫到这份文件并失败：

```text
Malformed board YAML file: .../usp_zephyr/boards/seeed/xiao_nrf54l15/board.yml
```

不要直接改 `usp_zephyr/`。RZI backend 的兼容问题由 RZI 通过官方
`west patch` 维护，登记在 `rzi/zephyr/patches.yml`，用法见
[west-patch.md](./west-patch.md)。`./scripts/container.sh build` 会先执行
`west patch -sm rzi clean`，再执行 `apply --roll-back`。`west update`
会把模块重置到清单 revision，补丁需要重新应用。

---

## 第 6 步：清掉旧 `/app` 的 CMake 缓存再编

以前产物在容器路径 `/workdir/build/app`。改挂载后源码是 `/workdir/app`，旧 `CMakeCache.txt` 会报目录不一致。

本机若直接跑过 `west build`，缓存里会是 `/home/daniel/rzi-workspace/zephyr`。容器里再 `-p always` 会去跑这份不存在的 `pristine.cmake` 而失败。`./scripts/container.sh build` 会在进容器前删掉宿主机上的 `build/app`。

```bash
rm -rf build/app
./scripts/container.sh build
```

已验证：Zephyr **4.4.99**，板 `rak4631/nrf52840`，产物 `build/app/zephyr/zephyr.elf`（另有 `.hex`）。

旧终端若还开着 **改挂载之前** 的 `./scripts/container.sh shell`，先 `exit` 再进；那个容器仍按旧的 `zephyrproject` + `/app` 挂载。

---

## 第 7 步：官方 USP sample（可选）

```bash
./scripts/container.sh sample
```

等价于：

```bash
west build -p always \
  -b nrf52840dk/nrf52840 \
  --shield semtech_sx1261mb2bas \
  -d /workdir/build-periodical-uplink \
  usp_zephyr/samples/usp/lbm/periodical_uplink
```

这是 **nRF52840 DK + Semtech SX126x 盾**，**不是 RAK4631**。WisBlock 板上的 SX1262 要另写 overlay；官方没有 `rak4631` 盾名。

---

## 不要做的事

- 不要 `import: true` 加在 `usp_zephyr` 上（会改去 Zephyr v4.2.0）。
- 不要改 `zephyr/samples/` 或把业务写进 `usp_zephyr/`。
- 不要 `west init -l /workdir`（`.west` 会落到 `/`）。
- 不要 `git config --global`；HTTP/1.1 已由脚本注入。
- 不要指望 USB 线 `west flash` 这份 hex；需要 SWD（J-Link / RAKDAP1 等）。
- 不要和 Zephyr 自带的 `CONFIG_LORAWAN` / in-tree loramac-node 叠在同一颗 SX1262 上。

---

## 日常

```bash
cd /home/daniel/rzi
chmod +x scripts/container.sh          # 只需一次
./scripts/container.sh build-image     # 镜像没有时做一次
./scripts/container.sh init            # 已有 .west 会跳过
./scripts/container.sh build           # 编 app/，rak4631
./scripts/container.sh sample          # 编官方 periodical_uplink
./scripts/container.sh shell           # 工作目录 /workdir
```

进容器后更新依赖：`west update`。只更新 USP：`west update usp_zephyr usp`。
