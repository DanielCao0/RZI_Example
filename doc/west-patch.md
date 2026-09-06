# Zephyr `west patch`：要求和用法

`west patch` 是 **Zephyr 自带的 west 扩展命令**（`zephyr/scripts/west_commands/patch.py`），给 [T2 工作区](./west-topology.md) 里被 `west update` 拉下来的模块打补丁。清单仓（本仓库是 `app/`）登记补丁元数据；模块仓（`usp_zephyr/`、`zephyr/` 等）保持干净，不要在里面直接 commit。

字段校验见 Zephyr 源码：`zephyr/scripts/schemas/patch-schema.yml`。官方说明：`zephyr/doc/develop/west/zephyr-cmds.rst`（Working with patches）。

本仓库 west 在 Docker 里，命令前加 `./scripts/container.sh`，或先 `./scripts/container.sh shell`。

## 解决什么问题

`west.yml` 只能钉某个 revision。模块和当前 Zephyr 对不上时（API 变了、缺文件、告警），需要本地改动，但：

- 改动不能 commit 进 `usp_zephyr/`：下一次 `west update` 会按清单把树重置掉。
- 不能靠手改源码记住差异：别人 clone 后编不过。

`west patch` 把「改了什么、打到哪个模块、有没有上游」写进清单仓，升级 Zephyr / 模块时可以一张张决定留下还是丢掉。

`west.yml` **没有** `patch:` 字段。补丁不写进清单的 `projects`，只写在下面说的 `patches.yml`。

## 目录要求

补丁定义默认在**清单仓**下，不在工作区根的 `zephyr/` 源码树里。

官方约定（相对清单仓）：

```text
<清单仓>/
  west.yml
  zephyr/
    patches.yml          # 元数据（默认路径）
    patches/             # 补丁文件目录（默认路径）
      <模块名>/
        0001-....patch
```

本仓库清单仓是 `app/`（`west init -l app`），因此实际路径是：

```text
rzi/
  app/
    west.yml
    zephyr/
      patches.yml
      patches/
        usp_zephyr/
          0001-zephyr-4.4-warning-fixes.patch
          0002-fix-lr-fhss-src-path.patch
  zephyr/                # Zephyr 源码，不是上面那个 zephyr/
  usp_zephyr/            # 被打补丁的模块
```

`zephyr/` **不是** Zephyr 源码。只是 west 默认用的名字：`WEST_PATCH_YAML = zephyr/patches.yml`，`WEST_PATCH_BASE = zephyr/patches`。

`patches.yml` 里每张补丁的 `path` 相对 `patches/`，不是相对工作区根：

| `path` | 磁盘上的文件 |
|--------|----------------|
| `usp_zephyr/0001-....patch` | `zephyr/patches/usp_zephyr/0001-....patch` |

`module` 是**工作区根下的目录**（或 Zephyr module 名），west 在那个目录里执行 `git apply`：

| `module` | 打到哪里 |
|----------|----------|
| `usp_zephyr` | `rzi/usp_zephyr/` |
| `zephyr` | `rzi/zephyr/` |
| `bootloader/mcuboot` | `rzi/bootloader/mcuboot/` |

可用 `-l` / `-b` 改 `patches.yml` 和补丁目录，用 `-sm` 指定「补丁定义在哪个模块」。默认：清单仓 + 上面两行路径。

## `patches.yml` 字段要求

文件用 [pykwalify](https://github.com/gmr/pykwalify) 校验。格式错了 `west patch` 直接退出。

### 顶层

| 字段 | 必填 | 默认 | 含义 |
|------|------|------|------|
| `patches` | 否 | 空 | 补丁列表，按**书写顺序**打 |
| `checkout-command` | 否 | `git checkout .` | `west patch clean` 时撤销已跟踪文件的改动 |
| `clean-command` | 否 | `git clean -d -f -x` | `west patch clean` 时删未跟踪文件 |

**本仓库把 `clean-command` 写成 `""`。** 默认的 `git clean -d -f -x` 会清掉模块里所有未跟踪文件（本地笔记、临时产物），不要用。

### 每一项（`patches:` 下的 map）

必填：

| 字段 | 规则 |
|------|------|
| `path` | 相对 `patches/` 的补丁文件路径 |
| `sha256sum` | 64 位小写十六进制。算法见下一节，**不要**用 `sha256sum` 命令的结果去碰换行不同的文件 |
| `module` | 工作区相对路径或模块名 |
| `author` | 作者显示名 |
| `email` | 必须含 `@` |
| `date` | `YYYY-MM-DD`（ISO 8601 日期） |

选填：

| 字段 | 默认 | 规则 / 用途 |
|------|------|-------------|
| `upstreamable` | `true` | 是否打算往上游提 |
| `merge-pr` | — | 上游 PR URL，须 `http://` 或 `https://` |
| `issue` | — | 上游 issue URL，同样须 http(s) |
| `merge-status` | — | PR 是否已合 |
| `merge-commit` | — | 已合入的 40 位 SHA-1 |
| `merge-date` | — | 合入日期 `YYYY-MM-DD` |
| `apply-command` | `git apply` | 在 `module` 目录里执行，后面自动接上补丁绝对路径 |
| `comments` | — | 给人看的说明 |
| `custom` | — | 任意 YAML，west 不校验 |

改过 `.patch` 文件后必须重算并更新 `sha256sum`，否则 apply 在打补丁之前就因校验失败退出。

## 校验和算法（要求）

west **不是**对磁盘字节做 `sha256sum`。它先当文本读入（统一换行），再按 UTF-8 哈希：

```python
with open(filename, encoding="utf-8", newline=None) as fp:
    content = fp.read()
hashlib.sha256(content.encode("utf-8")).hexdigest()
```

Linux 上 LF 文件，`sha256sum` 的结果通常一致。Windows 检出成 CRLF 时，系统 `sha256sum` 会和 west 对不上。新增或改补丁用下面这段，和 west 同一算法：

```bash
python3 -c "
import hashlib
p='zephyr/patches/usp_zephyr/0003-short-name.patch'
print(hashlib.sha256(open(p, encoding='utf-8', newline=None).read().encode()).hexdigest())
"
```

## 命令用法

都在工作区根执行（本仓库是 `rzi/`）。需要已经 `west update`，并且 `ZEPHYR_BASE` 指向工作区里的 `zephyr/`（Docker 脚本会设）。

| 命令 | 作用 |
|------|------|
| `west patch` / `west patch list` | 打印 `patches.yml` 里每一项 |
| `west patch apply` | 按列表顺序：校验 sha256 → 在对应 `module` 里跑 `apply-command` |
| `west patch apply --roll-back` | 有一张失败则对**已经打过的模块**跑一遍 clean |
| `west patch clean` | 对清单里出现过的每个 module 先 `checkout-command`，再 `clean-command` |
| `west patch gh-fetch` | 从 GitHub PR 拉 patch 并追加到 `patches.yml` |

常用全局参数（放在子命令**前面**）：

```bash
west patch -dm usp_zephyr apply          # 只处理这个目标模块
west patch -l zephyr/patches.yml apply
```

### apply 的行为

- **不是幂等的。** 已经打过再 `apply`，`git apply` 会失败。要重打：先 `clean` 再 `apply`。
- 按 `patches.yml` 顺序一张张打。中间失败就停，默认**不**自动回滚（除非 `--roll-back`）。
- 只改工作区工作树，**不会**在模块仓里生成 commit。

### clean 的行为

默认等价于在每个相关模块里：

```bash
git checkout .
git clean -d -f -x
```

会丢掉该模块里所有未提交改动。本仓库只用 `git checkout .`。

`west update` 会按 `west.yml` 把模块重置到清单 revision，效果上补丁也没了，需要再 `apply`。

### 推荐顺序

第一次或日常编译前：

```bash
west update
west patch apply
```

升级 Zephyr / `usp_zephyr` 之后：

```bash
west patch clean
west update
west patch apply --roll-back
```

某张已经进上游、当前 revision 已包含：从 `patches.yml` 删掉该项，删掉 `.patch`，再 `clean` + `update`。

从 GitHub PR 拉补丁：

```bash
west patch gh-fetch --owner Lora-net --repo usp_zephyr --pull-request <号> --module usp_zephyr
# 按 commit 拆成多张：
west patch gh-fetch --owner Lora-net --repo usp_zephyr --pull-request <号> \
  --module usp_zephyr --split-commits
```

需要 GitHub token 时设 `GITHUB_TOKEN`，或 `--token <文件>`。`gh-fetch` 会改 `patches.yml` 并写出 patch 文件，拉完仍要自己检查 `module`、`path`、能否 apply。

## 本仓库怎么用

封装（内部是 `west patch clean` 再 `west patch apply`，所以可以反复跑）：

```bash
./scripts/container.sh patch          # 撤掉再打上
./scripts/container.sh patch-list
./scripts/container.sh build          # 编 app 前会自动打补丁
```

当前登记的补丁（打在 `usp_zephyr`）：

| path | 作用 |
|------|------|
| `usp_zephyr/0001-zephyr-4.4-warning-fixes.patch` | Zephyr 4.4：SPI delay、分区宏、`nvs.h`、未用 LED |
| `usp_zephyr/0002-fix-lr-fhss-src-path.patch` | LR-FHSS 源码路径，不改则编不过 |
| `usp_zephyr/0003-xiao-nrf54l15-full-name.patch` | Xiao 板 board.yml 缺 `full_name`，扫板失败 |
| `usp_zephyr/0004-sx1262-pa-compile-definitions.patch` | SX1262 型号宏传到 BSP，否则功放按 SX1261 配 |

清单文件：`zephyr/patches.yml`。

## 新增一张补丁

1. 在目标模块里改代码，确认能编过。
2. 生成 unified diff（在该模块仓库根，路径才对）：

   ```bash
   git -C usp_zephyr diff > zephyr/patches/usp_zephyr/0003-short-name.patch
   ```

3. 用上一节的 Python 算 `sha256sum`。
4. 在 `patches.yml` 的 `patches:` 末尾加一项：至少 `path`、`sha256sum`、`module`、`author`、`email`、`date`。
5. `./scripts/container.sh patch`。若模块里还留着刚才的手改，先 `west patch clean`（或脚本里的 clean），否则 `git apply` 会报 already applied。

## 常见失败

| 现象 | 原因 |
|------|------|
| `sha256 mismatch` | 改了 `.patch` 没改 yml；或用了系统 `sha256sum` 且换行不一致 |
| `Malformed yaml` | 缺必填字段、`email` 没有 `@`、`date` 不是 `YYYY-MM-DD`、`sha256sum` 不是 64 位小写 hex |
| `git apply` 失败 / already exists | 补丁已打过，或模块 revision 变了、上下文对不上。先 `clean`；升级后对不上就要重做 diff |
| `no patches to apply: ... patches.yml not found` | 不在 west 工作区，或清单仓路径不是 `app/` |
| `west: unknown command "patch"` | 没 `west update`，或没注入 `ZEPHYR_BASE`（应在 Docker 里跑） |

## 不要做的事

- 不要把改动 commit 进 `usp_zephyr/` / `zephyr/` 再指望 `west update` 还在。
- 不要只手改模块、不导出 `.patch`：`clean` 或 `update` 会丢掉。
- 不要把 `clean-command` 留成默认的 `git clean -d -f -x`。
- 不要改完 `.patch` 忘记更新 `sha256sum`。
- 不要对已经 apply 过的树再跑一次 `west patch apply`（脚本的 `patch` / `build` 会先 clean，可以直接用）。
