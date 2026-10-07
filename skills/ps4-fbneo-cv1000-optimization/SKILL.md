---
name: ps4-fbneo-cv1000-optimization
description: 继续 PS4 RetroArch FBNeo 内核的 CV1000（Cave SH3 街机）性能优化、回归验证与 PKG 打包。用于怒首领蜂大往生/大复活、虫姬等 ddpsdoj / ddpdfk / ibara 掉帧定位、SH3 JIT 与 epic12 渲染层改动、宿主演进重放差分、以及 OpenOrbis 打包发布；不用于普通 ROM 管理、其他主机移植或 MAME2003-Plus。
---

# PS4 FBNeo CV1000 优化

单一正式源码：`/mnt/e/Projects/retroarch-ps4-upstream-sync/testbuild/fbneo-sh3-jit`，
分支 `ps4-cv1k-demand-alpha-20261005`，远端 `git@github.com:liujiny/FBNeo.git`。
**只推这一个分支，不新开分支、不 force、不改 remote。**

仓库里 `docs/PS4_CV1000_OPTIMIZATION_PLAYBOOK.md` 是本项目的方法论、验证门和教训全集。
**先读它，不要在这里重复造轮子。** 本技能负责路由、当前状态和运行入口。

- 现在到哪一步了、源码/索引/成品在哪 → [当前状态](references/current-state.md)
- 继续优化：先量后改、双序测量、什么算保留 → [优化流程](references/optimization.md)
- 出 PKG、验证门、Title ID → [打包](references/packaging.md)
- 快速看当前 HEAD、索引与最新包 → `scripts/ps4_fbneo_status.sh`

## 硬约束

JIT **默认关闭**，要显式 `-DFBNEO_SH3_X64_JIT=1`；运行期还有 `fbneo_sh3_jit` 选项（缺省
`disabled`）。"编进去了"和"跑起来了"是两件事，报告里必须分开写。

宿主重放（`tests/console_replay`）**不等于 PS4 帧率**，宿主 ±0.5% 一律当噪声。
只有用户实机确认才算性能结论。

一次只改一个可独立解释的东西，保留父提交与来源 SHA。所有对比都相对同一父提交。
**宁可判「不保留」，不要把噪声当收益**；未保留的方向要写清理由并进索引，防止重复试验。

动手前先查 `testbuild/continuous-optimization-index.json` 的 `rounds`，已证伪的方向不要重试。
参考仓库常含未提交的优化和私有资料，**不能用 `clean`/`reset`/`checkout` 去"弄干净"**。

状态必须分级写：`用户实机确认` / `模拟器验证` / `仅编译成功` / `失败已回退` / `待测试`。
不提交 ROM、SDK、密钥、大型日志或二进制；诊断代码不进正式树。
