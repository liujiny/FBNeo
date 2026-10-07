# 优化流程

方法论、验证门、实机 klog 调试技巧的**全文**在源码仓库
`docs/PS4_CV1000_OPTIMIZATION_PLAYBOOK.md`（374 行，中文）。
**开工前读那一份**，这里只写本技能必须强调的约束和入口。

## 六步循环（缺一步就不许合并）

1. **基线固定** —— 记录 HEAD、工作区是否干净、工具链、成品哈希；所有对比相对同一父提交。
2. **先量后改** —— 先用 profile / 采样 / 计数确认瓶颈，不做"看起来会更快"的改动。
3. **最小改动** —— 一次只改一个可独立解释的东西，留来源 SHA 和父子关系。
4. **等价性证明** —— 性能改动必须证明行为不变，否则先修正确性。
5. **双序测量** —— 同一候选在两种顺序下重复测量，只看**可重复**的收益；噪声大就判不保留。
6. **保留或回退** —— 保留留 `REPORT.md` + `retained.json`；回退写清拒绝理由并进索引。

## 构建与回归入口

```bash
cd /mnt/e/Projects/retroarch-ps4-upstream-sync/testbuild/fbneo-sh3-jit/src/burner/libretro
make platform=orbis-dynamic clean
make platform=orbis-dynamic "CXX=clang++ -DFBNEO_SH3_X64_JIT=1" -j4 orbis-self

cd ../../../
python3 tests/sh3_x64/run.py --sanitize --output /tmp/sh3-x64
python3 tests/console_replay/...      # 见 tests/console_replay/README.md
```

JIT 编译默认关闭；运行期选项 `fbneo_sh3_jit` 缺省 `disabled`。**不要改这两个默认开关**，
它们是回退手段。

## 两个特别有用的技巧（详见手册 §7.1）

* **纯解释器对象的字节隔离证明**：拿单个 `.o` 的哈希变化证明"哪个源文件真的进了产物"。
  只比对最终 `.self` 哈希不够 —— 它同时受链接顺序影响。
* **FFT / toggle 生命周期一致性**：音频与开关状态在快照前后必须一致，否则差分结论无效。

## 已知边界

* 未对齐取指 UB：`sh3_cpu_readop16()` 对 `MemMapF` 无 NULL/handler 守卫，PC 跑飞即 SIGSEGV。
  HEAD 基线在 Clang sanitizer 下同样崩 —— 所以 sanitizer 报这个不代表你的改动有问题。
* Sanitizer 有已知边界（手册 §7.2），不要拿它的输出直接当回归结论。
* 金手指与崩溃的因果**未闭合**：cheat 写入只走 `MemMapW`，不直接碰 `MemMapF` 或 `m_pc`；
  要定性必须做"关掉金手指同场景复现"的 A/B。
* `/data/retroarch/system/fbneo/cheats` 下没有 per-game ini 是**正常的** —— FBNeo 走
  `cheat.dat`（`ConfigCheatLoad()` → `ConfigParseMAMEFile()`），libretro 侧再转成 core option。

## 已证伪的方向（别重试）

手册 §11 列了完整清单，动手前先查
`testbuild/continuous-optimization-index.json` 的 `rounds`。大类包括：

* SH3/JIT：`sh3-call-terminal`、`sh3-call-delay`、`sh3-pr-threaded`、`sh3-codegen`、
  `sh3-simd-tail`、`sh3-imm8`、`sh3-regcache8`、`sh3-block-layout`、`sh3-lookup-reuse`、
  `sh3-nop-delay`、`sh3-div1`、`sh3-byte-loops`、`sh3-byte-outline` 等
* CV1000 渲染：`cv1k-alpha512`、`cv1k-tint-recip`、`cv1k-packed-add`、`cv1k-tint-shift`、
  `cv1k-alpha-tiles`、`cv1k-alpha-halfrows`
* 已准备但未应用：`cv1k-alpha-querysimd`（JIT 预取候选因 Ibara P95 +7.43% 被否）

## 保留的改动分四层

手册 §6 按 `SH3 解释/调度层`、`SH3 → x86-64 JIT`、`CV1000 渲染层`、`运行/构建基础设施`
分组列出。改任何一层之前先看那一层已经有什么，避免重复或互相抵消。
