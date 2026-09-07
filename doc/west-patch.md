# RZI `west patch` 使用说明

RZI 默认 backend 所需的兼容补丁由独立的 `rzi/` 模块维护，不再由客户应用复制
或维护。补丁清单位于 `rzi/zephyr/patches.yml`，补丁文件位于
`rzi/zephyr/patches/`。

本仓库的容器脚本会在 `init`、`shell`、`build` 和 `sample` 前自动执行：

```bash
west patch -sm rzi clean
west patch -sm rzi apply --roll-back
```

因此使用标准入口时不需要手工处理补丁：

```bash
./scripts/container.sh build
```

也可以显式查看或重新应用：

```bash
./scripts/container.sh patch-list
./scripts/container.sh patch
```

`-sm rzi` 是 `--src-module rzi`，表示从 RZI 读取补丁定义。补丁实际应用到哪个
模块由 `patches.yml` 中的 `module` 字段决定，目前目标是 `usp_zephyr`。

`apply` 不是幂等操作，所以自动化入口总是先 `clean` 再 `apply`。
`--roll-back` 会在某张补丁失败时清理本次已经修改过的目标模块。

注意：RZI 清单当前使用 `git checkout .` 清理目标模块，这会丢弃
`usp_zephyr` 等目标模块中已跟踪文件的未提交修改，但不会删除未跟踪文件。

补丁维护、校验和生成及完整命令说明见 RZI 仓库的
`doc/west-patch.md`。客户应用只负责调用，不拥有这些补丁。
