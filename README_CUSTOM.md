# GBL Root Canoe - Custom Build

基于 [superturtlee/gbl_root_canoe](https://github.com/superturtlee/gbl_root_canoe) 的自定义版本

## 🎯 主要特性

### ✅ 完全开放的 Fastboot 权限
- **绕过 OEM 解锁检查** - 无需开启"OEM 解锁"即可进入 fastboot
- **绕过 FRP 保护** - FRP 分区损坏时仍可刷机
- **完全开放 flash 权限** - 允许所有 `fastboot flash/erase/unlock` 命令

### ✅ Patch 7: 隐藏 Orange State 警告
- 跳过"Your device has been unlocked"警告页
- 跳过 5 秒倒计时
- 直接进入系统或 fastboot

### ✅ 使用上游最新链接方案
- 基于 commit `40e0d8b` (Improve Linking)
- 使用 ELF 注入替代巨大的 C 数组
- 解决 FV 空间不足问题

## 📋 与上游的差异

| 功能 | 上游版本 | 本版本 |
|------|---------|--------|
| OEM 解锁检查 | ✅ 保留 | ❌ 完全绕过 |
| FRP 保护 | ✅ 保留 | ❌ 完全绕过 |
| Orange 警告页 | ✅ 显示 | ❌ 隐藏 |
| Fastboot 权限 | 🔒 受限 | 🔓 完全开放 |
| 链接方案 | ✅ ELF 注入 | ✅ ELF 注入 |

## ⚠️ 安全警告

**本版本完全绕过以下 Android 安全机制：**
1. OEM 解锁开关
2. FRP (Factory Reset Protection)
3. Bootloader 解锁警告

**仅用于以下场景：**
- ✅ 开发测试设备
- ✅ 救砖场景
- ✅ FRP/OEM unlock 状态异常的设备

**不适用于：**
- ❌ 生产环境
- ❌ 日用设备
- ❌ 金融/支付应用设备
- ❌ 需要 Verified Boot 保护的设备

## 🔧 构建

```bash
# 1. 克隆仓库
git clone https://github.com/2481849298/gbl_root_canoe_custom.git
cd gbl_root_canoe_custom

# 2. 放置 ABL 镜像
cp /path/to/abl.img images/

# 3. 构建
make clean && make build

# 4. 输出文件
# dist/ABL_with_superfastboot.efi
```

## 📝 应用的补丁

### Patch 1: GBL (efisp 引用)
- ❌ 失败 (OPPO/OnePlus 设备不需要)

### Patch 2: device_state
- ✅ 成功 - `unlocked` → `locked`

### Patch 6: Lock State 字符串跳转
- ✅ 成功 - 跳过锁定状态检查

### Patch 7: Orange State 警告 (自定义)
- ✅ 成功 - 隐藏解锁警告页

### Patch: Boot State
- ✅ 成功 - 修改 LDRB/STRB 指令

## 📊 构建日志

```
Patch 7 fallback: orange state CBZ at 0x76D8 -> B
  Before: 6A 04 00 34
  After : 23 00 00
Patch 7: applied 1 location(s)

FV Space Information
FVMAIN [99%Full] 188608 total, 188592 used, 16 free
FVMAIN_COMPACT [15%Full] 249856 total, 38360 used, 211496 free
```

## 🔗 相关链接

- **上游仓库**: [superturtlee/gbl_root_canoe](https://github.com/superturtlee/gbl_root_canoe)
- **Vivy 分支**: [1vivy/gbl_root_canoe](https://github.com/1vivy/gbl_root_canoe)

## 📜 许可证

继承上游项目的许可证。详见 [LICENSE](LICENSE)

## 🙏 致谢

- [superturtlee](https://github.com/superturtlee) - 原始项目
- [1vivy](https://github.com/1vivy) - Patch 7 实现
