# usp_zephyr 接入清单

把一块带 SX1262 的板子跑上 Semtech USP / LoRa Basics Modem，一共要动 **6 个地方**。
本仓库 `app/` 就是一份完整实例（RAK4631），本文逐条对应。

| # | 文件 | 干什么 |
|---|------|--------|
| 1 | `west.yml` | 把 `usp_zephyr` 和 `usp` 拉进工作区 |
| 2 | `app/prj.conf` | 开 USP Kconfig，关掉 Zephyr 自带 LoRa |
| 3 | `app/boards/<board>.overlay` | 电台节点改 USP binding + 密钥 |
| 4 | `app/CMakeLists.txt` | 链接 LBM 编译定义 |
| 5 | `app/src/main.c` | 初始化顺序 + 事件回调 |
| 6 | `zephyr/patches.yml` | 给 usp_zephyr 打本地补丁 |

---

## 1. west.yml：加两个项目

```yaml
manifest:
  self:
    path: app
  remotes:
    - name: zephyrproject-rtos
      url-base: https://github.com/zephyrproject-rtos
    - name: lora-net
      url-base: https://github.com/Lora-net
  projects:
    - name: zephyr
      remote: zephyrproject-rtos
      revision: 161f758ba363ec90cd9b727a5d82e6c86efa85ad   # 钉 commit
      import: true
    - name: usp_zephyr
      remote: lora-net
      revision: main
      path: usp_zephyr
      # 不要写 import: true！否则会并入它自带清单里锁死的 Zephyr v4.2.0
    - name: usp
      remote: lora-net
      revision: main
      path: modules/lib/usp      # 必须是这个路径，Zephyr/CMake 按约定找
      submodules: true           # 不拉会缺无线电驱动源码
```

三条铁律：

1. **`usp_zephyr` 不写 `import: true`**。它的 `west.yml` 会把 Zephyr 钉在 v4.2.0，和你自己的 Zephyr 冲突。
2. **`usp` 必须放 `modules/lib/usp`**，且 `submodules: true`。
3. Zephyr 版本自己钉（本仓库用 4.4.99 的某个 commit），不要跟 usp_zephyr 的清单走。

初始化：`west init -l app && west update`（本仓库用 `./scripts/container.sh init`）。

## 2. prj.conf：Kconfig 开关

```conf
# 关掉 Zephyr 自带 LoRa/LoRaWAN，否则和 USP 抢同一颗 SX1262
CONFIG_LORA=n
CONFIG_LORAWAN=n

# USP 驱动 + 协议栈
CONFIG_LORA_BASICS_MODEM_DRIVERS=y
CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_NO_THREAD=y
CONFIG_USP=y
CONFIG_USP_LORA_BASICS_MODEM=y
CONFIG_USP_MAIN_THREAD=y

# USP 依赖的外设/服务
CONFIG_GPIO=y                      # DIO / reset / busy
CONFIG_SPI=y                       # 和 SX1262 通信
CONFIG_FLASH=y                     # LBM 把 Join 会话写 NVM
CONFIG_PM_DEVICE=y                 # 电台驱动要设备级电源管理
CONFIG_TEST_RANDOM_GENERATOR=y     # Join DevNonce 需要随机数

# 线程优先级：USP(-4) > main(-2)，main 让出后 USP 立刻跑
CONFIG_MAIN_THREAD_PRIORITY=-2
CONFIG_USP_MAIN_THREAD_PRIORITY=-4
```

依赖关系（`usp_zephyr/subsys/usp/Kconfig`）：

- `USP` 依赖 `LORA_BASICS_MODEM_DRIVERS`，后者要求 `!LORA`——所以必须先关 `CONFIG_LORA`。
- `EVENT_TRIGGER_NO_THREAD`：DIO 中断直接回调，不经中间线程，RX 窗口时间戳更准。
- `USP_MAIN_THREAD`：自动起一条线程跑 `smtc_modem_run_engine()` + `smtc_rac_run_engine()`，应用不用自己轮询。

## 3. overlay：电台节点改 USP binding

USP 用的是自己的 binding `semtech,sx1262-new`
（`usp_zephyr/dts/bindings/usp/semtech,sx126x-new-common.yaml`），
**不是** Zephyr 树上的 `semtech,sx1262`。以 RAK4631 为例：

```dts
/ {
	chosen {
		/* USP 线程 / HAL 用 DT_CHOSEN(zephyr_lorawan_transceiver) 找电台 */
		zephyr,lorawan-transceiver = &lora;
	};
	aliases {
		lora-transceiver = &lora;   /* USP sample 用 DT_ALIAS(lora_transceiver) */
	};
};

&lora {
	compatible = "semtech,sx1262-new";
	dio1-gpios = <&gpio1 15 (GPIO_ACTIVE_HIGH | GPIO_PULL_DOWN)>;
	/* 新 binding 才有的属性 */
	dio2-as-rf-switch;
	dio3-as-tcxo-control;
	tcxo-voltage = <SX126X_TCXO_SUPPLY_3_3V>;
	tcxo-wakeup-time = <5>;
	reg-mode = <SX126X_REG_MODE_DCDC>;
	/* 删掉树上旧 binding 的属性，否则冲突 */
	/delete-property/ dio2-tx-enable;
	/delete-property/ dio3-tcxo-voltage;
	/delete-property/ tcxo-power-startup-delay-ms;
	/delete-property/ rx-enable-gpios;
	/delete-property/ label;
};
```

新旧 binding 对照：

| 树上 `semtech,sx1262` | USP `semtech,sx1262-new` |
|-----------------------|--------------------------|
| `dio2-tx-enable` | `dio2-as-rf-switch` |
| `dio3-tcxo-voltage = <...>` | `dio3-as-tcxo-control` + `tcxo-voltage = <SX126X_TCXO_SUPPLY_*>` |
| `tcxo-power-startup-delay-ms` | `tcxo-wakeup-time` |
| `rx-enable-gpios` | **没有**，见下面 gpio-hog |
| `label` | 废弃，删掉 |

常量头文件（`SX126X_TCXO_SUPPLY_*`、`SX126X_REG_MODE_*`）在
`usp_zephyr` 的 `zephyr/dt-bindings/usp/sx126x.h`，overlay 顶部要
`#include <zephyr/dt-bindings/usp/sx126x.h>`。

### P1.05：RF 开关电源常高

RAK4631 的 P1.05 是 RF 开关电源，TX/RX 都要保持高电平。天线切换走 SX1262 DIO2
（`dio2-as-rf-switch`）。树上写成 `rx-enable-gpios` 是错的；USP binding 也没有
这个属性，用 gpio-hog 常高：

```dts
&gpio1 {
	antenna-power-hog {
		gpio-hog;
		gpios = <5 GPIO_ACTIVE_HIGH>;
		output-high;
	};
};
```

不配或电平错了，能发不能收，Class A 入网会一直 JOINFAIL。

全片擦除后 nRF52840 的 UICR `REGOUT0` 默认 1.8 V，SX1262 控制脚会失效。
`src/boards/rak4631.c` 的 `board_early_init_hook` 在启动时把它写成 3.3 V 并复位。

### 密钥节点 `zephyr,user`

`zephyr,user` 是 Zephyr 通用杂项节点；`user-lorawan-*` 是 USP sample / 本 app 的约定名，
编译期变成宏，由 `main.c` 读：

```dts
	zephyr,user {
		user-lorawan-device-eui = <0x12 0x34 ...>;   /* 8 字节 */
		user-lorawan-join-eui  = <0x00 ...>;         /* 8 字节 */
		user-lorawan-gen_app-key = <...>;            /* 16 字节，1.1 AppKey */
		user-lorawan-app-key   = <...>;              /* 16 字节，1.0 AppKey → set_nwkkey */
		user-lorawan-region = "EU_868";              /* 对应 SMTC_MODEM_REGION_* */
	};
```

## 4. CMakeLists.txt：链接 LBM 编译定义

```cmake
target_sources(app PRIVATE src/main.c)
target_link_libraries(app PRIVATE lbm_compile_definitions)
```

`lbm_compile_definitions` 是 usp_zephyr 导出的 CMake target，不带它 LBM 的条件编译宏不全。

## 5. main.c：初始化顺序与事件

```c
LOG_MODULE_REGISTER(usp, LOG_LEVEL_INF);   /* 模块名必须是 usp，USP 线程才能链接 */

int main(void)
{
	SMTC_SW_PLATFORM_INIT();                            /* 等 USP 线程 HAL 就绪 */
	SMTC_SW_PLATFORM_VOID(smtc_rac_init());
	SMTC_SW_PLATFORM_VOID(smtc_modem_init(&modem_event_callback));
	k_sleep(K_FOREVER);        /* 引擎在 USP 线程里跑，main 睡死即可 */
}
```

入网在 **RESET 事件** 里做（不是 main 里直接调）：

```c
case SMTC_MODEM_EVENT_RESET:
	smtc_modem_set_deveui(STACK_ID, user_dev_eui);
	smtc_modem_set_joineui(STACK_ID, user_join_eui);
	smtc_modem_set_appkey(STACK_ID, user_gen_app_key);   /* 1.1 AppKey */
	smtc_modem_set_nwkkey(STACK_ID, user_app_key);       /* 1.0 AppKey */
	smtc_modem_set_region(STACK_ID, MODEM_REGION);
	smtc_modem_join_network(STACK_ID);
	break;
```

要点：

- `smtc_modem_init` / `join_network` 只是**下命令**；真正收发、开 RX 窗口、回调事件的是
  `CONFIG_USP_MAIN_THREAD` 起的那条 USP 线程。
- JOINFAIL 后 LBM 会自己按退避重试，应用不用再调 `join_network`。
- 区域字符串用宏拼枚举：
  `DT_CAT(SMTC_MODEM_REGION_, DT_STRING_UNQUOTED(DT_PATH(zephyr_user), user_lorawan_region))`。

## 6. patches.yml：给 usp_zephyr 打补丁

`usp_zephyr` 是 west 拉下来的仓，本地改动一 `west update` 就没。
用 Zephyr 官方 `west patch`：清单仓里放 `zephyr/patches.yml` + `zephyr/patches/*.patch`，
`west patch apply` 在每次 build 前自动打（本仓库由 `scripts/container.sh` 包办）。

当前补丁（详见 [west-patch.md](./west-patch.md)）：

| 补丁 | 不打的后果 |
|------|-----------|
| `0001-zephyr-4.4-warning-fixes` | Zephyr 4.4 上一堆 deprecated 告警 |
| `0002-fix-lr-fhss-src-path` | CMake 找不到 `lr_fhss_mac.c`，链接失败 |
| `0003-xiao-nrf54l15-full-name` | 板列表扫描失败，编任何板都报错 |
| `0004-sx1262-pa-compile-definitions` | BSP 按 SX1261 配功放（device_sel=1），发射偏弱 |

## 验证清单

```bash
./scripts/container.sh build      # 编译，FLASH 约 175 KB
./scripts/flash-rak4631.sh                    # 烧录
```

串口（USB CDC，115200）预期：

```text
rzi USP LoRaWAN on rak4631/nrf52840
DevEUI 1234567812345678
RESET: set keys, region, join
JOINED                     / JOINFAIL (check keys / region / gateway)
uplink #0 on port 1
```

| 现象 | 查什么 |
|------|--------|
| 编译报 `DT_CHOSEN(zephyr_lorawan_transceiver)` | overlay 的 `chosen` 没写 |
| 编译报 binding 属性不存在 | 旧 `semtech,sx1262` 属性没 `/delete-property/` |
| 一直 JOINFAIL，网关也看不到包 | 区域/信道与网关不一致 |
| 一直 JOINFAIL，网关能看到包 | 密钥、JoinEUI 不对，或 RX enable 没拉低 |
| 能发不能收 | RX enable 的 gpio-hog 没配 |
