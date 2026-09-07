# west 工作区拓扑：T1、T2、T3

west 一次管一堆 git 仓，必须有一个**清单仓**写 `west.yml`，决定拉谁、拉哪个 revision。Zephyr 按**谁当这个老板**分成三种拓扑（官方名：T1 / T2 / T3）。出处：`zephyr/doc/develop/west/workspaces.rst`。

「星形」= 中间一个老板，周围一圈项目都听它的清单。「森林」= 没有产品仓当中心，单独一个只放清单的仓，各个 app 和 Zephyr 平级。

| | 清单仓（老板） | 布局 | 什么情况下用 |
|--|----------------|------|----------------|
| **T1** | `zephyr/` 自己 | 星形 | 学 Zephyr、改内核、跟官方 sample；固件还不成「一个产品仓」 |
| **T2** | 你的应用仓（本仓库是 `app/`） | 星形 | **一份**产品固件，要自己钉 Zephyr / 模块版本，补丁跟固件一起走 |
| **T3** | 单独的 `manifest-repo/`，里面没有固件源码 | 森林 | **多份**互相独立的固件，要共用同一套依赖版本 |

选拓扑看两件事：**你有几个要发布的固件**，以及**版本和补丁应该跟谁一起提交**。

- 只有一份固件，业务代码才是中心 → **T2**（本仓库）。
- 还没有自己的产品仓，只是在 Zephyr 树里试、改驱动 → **T1**。
- 好几份固件（网关 / 节点 / 产测……），希望一次 `west.yml` 锁定全公司的 Zephyr，各 app 各自一个 git → **T3**。
- 不要为了「看起来专业」上 T3：多一个清单仓就要多维护一次 revision，一个人做一款板子用 T2 更简单。
- 不要把产品固件长期放在 T1：`west.yml` 在 `zephyr/` 里，你一 `west update` 容易被上游带着走；自己的 overlay、补丁也没有老板仓可放。

`west update` 只动 `projects` 里那些仓，**不会**改清单仓自己的提交。

工作区根（有 `.west/` 的那一层）**不能**再套一层 git。git 在各个子目录里，不在 `rzi/` 根上。

## T1：Zephyr 当老板

`west init` 不带参数、或 `-m` 指向官方 Zephyr 仓，得到的就是 T1。清单是 `zephyr/west.yml`，HAL、mcuboot 等模块都由它拉进来。你的应用只是旁边一个目录，不是老板。

```text
west-workspace/                 # 有 .west/
  zephyr/                       # 老板：.git + west.yml
    west.yml
    samples/hello_world/
  modules/
    hal/nordic/
    lib/picolibc/
```

类比：Git submodule，**超项目是 Zephyr**。

**用 T1 的情况**

- 按官方 Getting Started 搭环境，跑 `samples/hello_world`、USP 官方 sample。
- 给 Zephyr 提 PR、改驱动 / 板级文件，工作树就是上游那份。
- 临时试验，还没有「这是我们的产品仓」这件事。

**不要用 T1 的情况**

- 已经有自己的 `main.c`、overlay、密钥、发布节奏。清单在 `zephyr/` 里，产品改动和上游改动混在一个仓，`west update` 还会把你钉的模块冲掉。
- 需要 `west patch`：官方补丁约定挂在**清单仓**的 `zephyr/patches.yml`。T1 的清单仓就是 Zephyr 源码，补丁和上游树叠在一起，不适合当产品补丁仓库。

## T2：应用当老板

清单在应用仓里。`west.yml` 把 `zephyr` 和其它模块写成 `projects`。`west update` 按**你的**清单拉依赖；Zephyr 只是其中一项。

```text
west-workspace/
  application/                  # 老板：.git，west 不改这里
    CMakeLists.txt
    prj.conf
    src/main.c
    west.yml                    # 主清单
    zephyr/patches.yml          # 可选：清单仓自有的 west patch
  zephyr/                       # 被拉下来的项目
  modules/lib/...
```

类比：Git submodule，**超项目是应用**，Zephyr 是子模块。

官方示例（`import: true` 表示再读 `zephyr/west.yml`，HAL 等不用自己抄一遍）：

```yaml
manifest:
  remotes:
    - name: zephyrproject-rtos
      url-base: https://github.com/zephyrproject-rtos
  projects:
    - name: zephyr
      remote: zephyrproject-rtos
      revision: v2.5.0
      import: true
  self:
    path: application
```

`west patch` 可以把产品补丁放在应用清单仓，也可以通过 `-sm` 从一个共享模块
读取补丁定义。本工作区使用后者：RZI backend 补丁由 `rzi/` 维护。

**用 T2 的情况**

- 一个仓库对应一款（或一族很接近的）固件，比如本仓库的 RAK4631 USP 应用。
- 要自己决定 Zephyr 用 4.4、`usp_zephyr` 跟 main，而不是跟官方 Getting Started 默认那份走。
- 对模块的改动要进 git，但不想 fork：补丁放在应用仓，别人 `west init -l app` 就能复现。
- 小组就围着这一份 `app/` 开发，没有「多产品共用一张清单」的需求。

**不要用 T2 的情况**

- 同一套 Zephyr + 模块要供 3 个以上独立产品仓使用，每个仓各写一份 `west.yml` 会漂版本 → 考虑 T3。
- 你其实在改 Zephyr 本身并准备向上游提交 → 用 T1，在 Zephyr 树里工作。

### 本仓库是 T2

```text
rzi/                            # 工作区根（.west/）
  app/                          # 老板 = 官方图里的 application/
    west.yml
    src/main.c
    scripts/container.sh        # 自动调用 west patch -sm rzi
  rzi/
    zephyr/patches.yml          # RZI backend 补丁定义
  zephyr/
  usp_zephyr/
  modules/lib/usp/
```

`west init -l app` 的 `-l` 是 local：别去 GitHub 再 clone 一份清单仓，本地 `app/` 就是老板。`self.path: app` 和 `.west/config` 里的 `path = app` 对上。

不要给 `usp_zephyr` 写 `import: true`，否则会并入它自带清单里锁死的 Zephyr v4.2.0，把你钉的 4.4 冲掉。

## T3：单独的清单仓（森林）

清单仓里**几乎只有** `west.yml`，没有固件。各个 app、Zephyr、模块都是 `projects`，平级，由这份清单一起拉。

```text
west-workspace/
  manifest-repo/                # 老板：只有 west.yml
    west.yml
  app1/                         # 也是 project，west 会按 revision 拉
  app2/
  zephyr/
  modules/lib/...
```

类比：Google repo 那种「清单仓 + 一堆平级仓」。本仓库不是 T3：`app/` 里既有清单也有源码。

### 再举一个跟本仓库更近的例子

先记住一句话：**T3 的老板仓是「目录」，不是「程序」。** 程序有好几份，各自一个 git。

假设公司后来不只有现在这份 RAK4631 节点，又多了两份固件，三个人各写各的：

| 人 | 产品 | git 仓里有什么 |
|----|------|----------------|
| 你 | RAK4631 节点（现在的 `app/`） | `src/main.c`、overlay、密钥 |
| 小李 | 网关 | 另一份 `src/main.c` |
| 小张 | 产测夹具 | 又一份 `src/main.c` |

如果继续用 **T2**，就是三份独立工程，每份都有自己的 `west.yml`，每份都写「Zephyr 用 4.4.99」。你升级了 Zephyr，小李忘了改，现场两套行为。三份清单会漂。

改成 **T3**，公司再开第四个仓，名字随便叫 `fw-manifest`，**整个仓就一个 `west.yml`，没有 `main.c`**：

```yaml
# fw-manifest/west.yml  — 这就是全部内容
manifest:
  projects:
    - name: zephyr
      revision: 161f758ba363ec90cd9b727a5d82e6c86efa85ad   # 全公司共用
      import: true
    - name: usp_zephyr
      revision: main
    - name: node          # 你现在这份固件，单独一个仓
      path: node
    - name: gateway       # 小李的
      path: gateway
    - name: factory       # 小张的
      path: factory
```

新同事第一天只 clone **这一本目录**：

```bash
west init -m git@company/fw-manifest.git
west update
```

`west update` 跑完，硬盘上同时出现：

```text
fw-ws/
  fw-manifest/          # 刚 clone 的老板，仍然没有 main
    west.yml
  node/                 # 你的仓，这里才有 int main(void)
    src/main.c
    boards/rak4631_nrf52840.overlay
  gateway/
    src/main.c
  factory/
    src/main.c
  zephyr/
  usp_zephyr/
```

然后：

```bash
west build -b rak4631/nrf52840 node       # 编你的节点
west build -b ... gateway                 # 编小李的网关
```

`fw-manifest` 自始至终没有 `main`，因为**没有人烧录「目录」**。烧录的是 `node` 编出来的 hex。

你改 join 逻辑：进 `node/`，commit 推到节点那个仓，**不是**推到 `fw-manifest`。
全公司升 Zephyr：只改 `fw-manifest/west.yml` 里那一行 SHA，三个人 `west update`，三份固件一起换版本。

本仓库现在是 T2，等于「目录写在节点仓自己身上」：只有你这一份 `main`，所以清单和 `main.c` 可以住在同一个 `app/` 里。等真的有第二份、第三份独立固件，又要锁同一版 Zephyr，才值得把目录拆出去变成 T3。

### T3 没有 `main`？`main` 不在老板仓里

清单仓**不是程序**，只是一张购物清单：告诉 west「去哪 clone、checkout 哪一版」。`main()` 在旁边那些应用仓里，`west update` 之后磁盘上已经有完整工程，编译指向那个目录即可。

对照：

| | T2（本仓库） | T3 |
|--|-------------|-----|
| 老板仓里有什么 | `west.yml` **加上** `src/main.c` | **只有** `west.yml`（最多再加点说明、CI） |
| `main()` 在哪 | 老板仓自己：`app/src/main.c` | 被拉下来的项目：`app1/src/main.c`、`app2/src/main.c` |
| 编谁 | `west build … app` | `west build … app1` 或 `app2` |
| 改业务代码 commit 到哪 | `app/`（老板仓） | `app1/` 或 `app2/`（各自的 git） |
| 改 Zephyr 版本 commit 到哪 | 还是 `west.yml` | `manifest-repo/west.yml` |

可以想成：T2 是「菜谱写在自家厨房里，菜也在自家炒」；T3 是「公司印一本总菜单（清单仓），各分店（app1、app2）各自有后厨和 `main`，大家都按同一本菜单进货（同一版 Zephyr）」。

清单仓自己没有 `int main`，**也不需要有**。没有人去 `west build manifest-repo`。

### T3 日常怎么用

假设公司 git 上有三个仓：

- `https://git.example.com/manifest` — 只有 `west.yml`
- `https://git.example.com/node-fw` — 节点固件，里面有 `src/main.c`
- `https://git.example.com/gateway-fw` — 网关固件，也有自己的 `main.c`

`manifest/west.yml` 类似：

```yaml
manifest:
  remotes:
    - name: zephyrproject-rtos
      url-base: https://github.com/zephyrproject-rtos
    - name: company
      url-base: https://git.example.com
  projects:
    - name: zephyr
      remote: zephyrproject-rtos
      revision: v4.1.0
      import: true
    - name: node-fw
      remote: company
      revision: v1.2.0
      path: app1
    - name: gateway-fw
      remote: company
      revision: v3.0.1
      path: app2
  self:
    path: manifest-repo
```

第一次：

```bash
west init -m https://git.example.com/manifest my-ws
cd my-ws
west update          # 拉 zephyr、app1、app2…
west build -b rak4631/nrf52840 app1     # 编节点，用的是 app1 里的 main
west build -b native_sim app2           # 编网关，用的是 app2 里的 main
```

`west init -m` 只 clone **清单仓**。应用仓是 `west update` 按 yml 再 clone 的，所以老板仓可以没有一行 C。

改节点逻辑：进 `app1/`，commit 推进 `node-fw` 那个仓，再把 `manifest-repo/west.yml` 里 `node-fw` 的 `revision` 改成新 tag。改 Zephyr 版本：只改清单仓的 `zephyr.revision`，两个产品下次 `west update` 一起换。

**用 T3 的情况**

- 多个独立应用仓（`app1`、`app2`），希望一次 bump Zephyr，所有产品一起换版本。
- 做下游发行版：对外发布的是「这份清单 + 指定 revision」，应用源码不塞进清单仓。
- 应用各自有自己的 git 权限 / 发布周期，只有依赖版本必须对齐。

**不要用 T3 的情况**

- 只有一个产品仓。多出来的 `manifest-repo` 没有收益，初始化还比 `west init -l app` 绕。
- 想把 overlay、补丁、密钥和 `main.c` 放在同一个仓里给人 clone → 那是 T2 的事。T3 的老板仓里通常没有这些。

## 怎么分辨自己是哪一种

看工作区里**哪一个目录带着 `west.yml` 且被 `.west/config` 指成 manifest**：

```text
# .west/config 里类似
[manifest]
path = app          # → T2（本仓库）
# path = zephyr     # → T1
# path = manifest-repo  # → T3
```

再看这个目录里有没有你的 `src/`、`CMakeLists.txt`：有 → 多半 T2；没有、只剩 yml → T3；清单就在 `zephyr/west.yml` → T1。

## 和本仓库其它文档的关系

- 初始化、为什么 `west init -l app`：[zephyr-docker-environment-explained.md](./zephyr-docker-environment-explained.md)
- 给 T2 里拉下来的模块打补丁：[west-patch.md](./west-patch.md)
