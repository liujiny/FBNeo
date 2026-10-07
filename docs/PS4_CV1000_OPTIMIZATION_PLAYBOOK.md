# PS4 FBNeo CV1000 优化与实机调试手册（AF 终版）

本文件把 CV1000（Cave SH3 街机）在 PS4 上从「能跑」推到「基本满帧」这一整轮工作中
**实际用到的方法、工具、验证门和纪律**整理在一起，供后续继续优化、回归排查或交接复用。

- 正式源码：本仓库，分支 `ps4-cv1k-demand-alpha-20261005`，终版提交 `11d50b202d5554fc9e4501a4257807b17d4a5af3`
- 终版实机包：`RetroArch_PS4_AF_TerminalBranch_SH3_RAPS10029.pkg`（Title `RAPS10029`，179306496 字节，SHA256 `1beb1515bac7b46522a7933740826abcf0ebb5512bada68adfcf6b5deec8476d`）
- 验证状态：`用户实机确认` —— ddpsdoj / ddpdfk 基本满帧，大场景也可超过 55 fps
- 逐个课题的单独记录见同目录 `PS4_*.md`

状态分级在本项目里始终分开写，不允许混用：

| 标记 | 含义 |
| --- | --- |
| 用户实机确认 | 真机跑过、用户认可 |
| 模拟器验证 | 宿主重放/差分，**不等于** PS4 帧率 |
| 仅编译成功 | 只证明能编过 |
| 失败／已回退 | 试过不行，已还原 |
| 待测试 | 代码在，结论没有 |

---

## 1. 总体方法论

一轮优化固定走这六步，任何一步缺失都不允许合并：

1. **基线固定**：先记录当前 HEAD、工作区是否干净、工具链、成品哈希。所有对比都相对同一父提交。
2. **先量后改**：先用诊断（profile / 采样 / 计数）确认瓶颈在哪，不做「看起来会更快的」改动。
3. **最小改动**：一次只改一个可独立解释的东西，保留来源 SHA 和父子关系。
4. **等价性证明**：性能改动必须证明**行为不变**（见第 4、7 节），否则先修正确性。
5. **双序测量**：同一候选在两种顺序下重复测量，只看**可重复**的收益；噪声大就判「不保留」。
6. **保留或回退**：保留要留 `REPORT.md` + `retained.json`；回退要写清拒绝理由，进索引防止重复试验。

核心纪律：**宁可判「不保留」，不要把噪声当收益**。大量候选都是「均值 +0.x%」这种量级，
本项目一律记为未保留，只留下有重复性收益的少数几项。

---

## 2. 构建环境

- 交叉工具链：OpenOrbis（`ORBISDEV=/usr/local/orbisdev`，`OO_PS4_TOOLCHAIN=/opt/openorbis`），必须装 `lld`。
- 构建在容器里做，保证可复现：Alpine 镜像 + `apk add --no-cache lld`，宿主项目目录以 `-v <project>:/project` 挂入。
- 正式产物链：`elf → oelf → self`，核心与前端分开构建、分开校验。
- 前端（RetroArch）在本轮**没有重新构建**，直接复用已验证的 Z 前端 `f26f487ecddfa79dff0bd8a77fcb141e4780a599`，字节一致。改核心时不动前端，能大幅缩小变量。
- 关键点：**Linux 原生对象绝不能和 PS4 对象混链**；宿主测试在另一份检出里做。

核心构建命令（`src/burner/libretro` 下）：

```
make platform=orbis-dynamic clean
make platform=orbis-dynamic "CXX=clang++ -DFBNEO_SH3_X64_JIT=1" -j4 orbis-self
```

JIT 编译默认关闭，用 `-DFBNEO_SH3_X64_JIT=1` 显式打开；运行期还有 `fbneo_sh3_jit` 选项（缺省 `disabled`），
意味着「编进去了」和「跑起来了」是两件事，报告里必须分开写。

---

## 3. 打包流水线

目录形如 `testbuild/<round>-pkg-logs/`，四阶段脚本，每阶段失败即断言退出：

| 脚本 | 干什么 |
| --- | --- |
| `prepare.py` | 从已验证的 stage 复制工作区；用结构体解析改写 `sce_sys/param.sfo` 的 `TITLE/TITLE_ID/CONTENT_ID`；同步改 `Project.gp4`；复制已验证前端；**逐个核对前端 payload 哈希**；落 `source-commit.txt`；拉起 Ubuntu 打包容器（`libssl1.1 libicu66`） |
| `compile.py` | 断言工作区干净、HEAD 等于 `source-commit.txt`；记录 `sh3_x64_jit.h` 哈希；容器内 clean 构建并落 `fbneo-clean-build.log` |
| `build-package.py` | 组包（PKG 生成 + 校验） |
| `finalize-report.py` | 汇总 `build_report.json`，把 ELF 符号表、各段哈希、溯源写进去 |

产物与证据一起留在目录里：`build_report.json`、`build-provenance.json`、`built-header-sha256.txt`、
`import-audit.json`、`object-module-audit.json`、`dynamic-audit.txt`、`frontend-heap-audit.log`、
`pkg-validate.log`、`packaged-param.sfo`。

---

## 4. 构建与打包验证门

组包前必须全绿，缺一项就不打包：

- **对象平台**：`object-module-audit.json` 要求 1140 个对象**全部** FreeBSD AMD64，`non_FreeBSD_objects` 必须为空。
- **导入符号**：`import-audit.json` 记录未定义符号集合，并与上一版比对（`undefined_symbols_equal_AB`），防止悄悄引入新依赖。
- **动态段**：`dynamic-audit.txt` 检查 `NEEDED`（只允许 `libkernel.so` / `libSceLibcInternal.so`）、PIE、RELA/RELACOUNT 等。
- **前端堆**：`frontend-heap-audit.log` 三段（elf / oelf / self）都要求 `ordinary/internal heap=256MiB` 且其余 CRT 字段保持。
- **SFO 语义**：改写后用 `packaged-param.sfo` 复核，不只看字节写成功。
- **PKG 校验**：`pkg-validate.log` 32 项全 `[OK]`（各类 digest、license、条目完整性）。
- **payload 哈希**：包内每个 payload 逐一比对来源哈希（18 项）。
- **溯源交叉验证**：拿**目标对象 SHA** 证明改动真的进了包。例如 AF 从 AE 出发，`sh4.o` 由 `5821b402…`
  变为 `10733f53…`（证明 JIT 头进了 CPU 对象），而 renderer 的 `epic12.o` 仍是 `a9652a2c…`（证明没被牵连）。

> 经验：只比对最终 `.self` 哈希是不够的——它同时受链接顺序影响。用**单个 `.o`** 的哈希变化来证明「哪个源文件进了产物」最可靠。

---

## 5. 性能测量

### 5.1 宿主重放（console_replay）

`tests/console_replay/` 是主要测量台：`headless.cpp` + `render_worker.cpp`，从 Salvia 的 PGM 重放台改写而来。

```
g++ -std=c++17 -O2 -ldl headless.cpp -o headless
./headless <core.so> <rom.zip> <output-dir> <frames> <depth> <avmask> <state-or--> <play> <hash> <reset> <stress> <lifecycle> <threads> <switch-threads>
```

- 输出 `hashes.txt` 与 `end.state`，与「未改动上游集成核心」在独立 worktree 里构建的对照比。
- `lifecycle=1`：第 300 帧复位、1200 存档、1600 读档、1800–1899 只跑音频。
- `switch-threads=1`：在 700/1400/2100 帧切换渲染线程数。
- 宿主构建用 `CXX="g++ -DFBNEO_RENDER_THREADS_TEST"`，走与 PS4 相同的 POSIX worker 和有界 CPU 等待路径。

**口径**：宿主重放的耗时和帧率**不是** PS4 的 FPS。所有宿主数字只用来判断「方向对不对」，
最终结论必须来自实机。

### 5.2 测量规范

- **双序 A/B**：同一候选正序、逆序各测一遍（`dual.json`），只认可重复方向。
- **慢帧窗口**：额外单独统计「同一组慢帧」的 CPU 时间（`slow-window.json`），避免全局均值掩盖尖刺。
- **P95**：均值之外必看 P95，防止「平均变好、卡顿变多」。
- **黄金哈希**：DDPSDOJ / DDPDFK / Ibara 三款的视频、音频、终态哈希必须逐字节不变。
- **重复**：关键结论跑 3 次以上（`repeat.json`）。
- **no-JIT 审计**：关闭 JIT 时，循环代码与已知表必须与父提交**字节相同**、符号重定位相同（`no-jit-audit.json`）。
  这条能证明改动只影响 JIT 路径。

AF 轮的宿主结果（600 帧、每变体 3 次、与解释器对比）：ddpsdoj 均值 1.6758 → 1.5010 ms（−10.43%），
P95 3.6397 → 3.1832；ddpdfk 2.2223 → 1.9500 ms（−12.26%）；ibara 1.6391 → 1.6150 ms（−1.47%，基本持平）。

### 5.3 诊断手段

先测再改，常用的几种：

- **慢指令 profile**：按操作码统计耗时，找最贵的指令族（`ad-slow-opcode-profile`）。
- **短循环 profile**：找被反复执行的短序列（`sh3-short-loop-profile`）。
- **CPU 采样**：12 次回放采样，按函数归因（JIT / dispatcher / ANON）（`current-cpu-samples`）。
- **可行性 profile**：估算某优化「理论上最多省多少像素读/查询」（`cv1k-alpha-demand-profile`、`cv1k-alpha-rows-profile`）。
- **有界分段计时**：把一帧切成有界阶段测时间，并采样 worker 可用性（`311b4efb1`）。

诊断代码**不留在正式树**：做完就还原，并用 `nm -D` 确认没有残留的 `retro_ad_*` 之类的导出符号。

---

## 6. 保留的优化（按层次分组）

### 6.1 SH3 解释/调度层

| 提交 | 内容 |
| --- | --- |
| `f9c92b88e` | 常用 SH3 指令改直接线程化 dispatch |
| `fe8ab8f02` | 纯 ALU 连续段之间保留 SH3 PC / cycle 状态 |
| `2bf238614`、`9d9bd77bc` | 预译码指令 handler dispatch（含 group 0/4） |
| `bed52d2ea` | 非 JIT 切片循环里批量处理受保护的 RAM 记账 |
| `c711071c6` | 在回调边界前批量累计 timer tick |
| `9614731d8` | 在 x86-64 上缓存精确的 prescaler 倒数 |
| `efb057ecb` | 减少 stopped timer 的 prescaler 迭代 |
| `a4ab81a3c` | 在 x86-64 上简化精确 DIV1 标志计算 |
| `6de3b434f` | SH2 挂起周期只推进到下一个待处理 timer |

### 6.2 SH3 → x86-64 JIT

| 提交 | 内容 |
| --- | --- |
| `293b2222b` | 实验性、可选启用的 SH3 x64 块编译器 |
| `46f0a06ea` | native region 扩展到 store 和条件延迟槽 |
| `3ee52a0ef` | 折叠 PC 相对加载的地址计算 |
| `dabc93383` | native region 之间保持 dispatcher 栈帧 |
| `e21ea8cda` | 整数乘法按精确额外周期编译 |
| `8baba6a52` | `DT` 带活读与忙等守卫的翻译 |
| `0d94b5389` | 把受保护的 native 退出移出成功路径 |
| `beb02a417` | SH3 编译移出 native 入口热路径 |
| `ca48f8c4e` | 给 JIT 校验加上界，并提供解释器回退选项 |
| `27476d177` | 8 MiB 代码 arena，避免重建风暴 |
| `d6b299ad0` | JIT fallback 中批量处理受保护的字节循环 |
| `eeba0514d` | 以标量块比较有界操作码尾巴 |
| `d22990c71` | native 字面量用常量位移寻址 |
| `c081e5d09` | 把终止分支和 PR 访问融合进 native region |

### 6.3 CV1000 渲染层

| 提交 | 内容 |
| --- | --- |
| `a80b459c6` | 从 PS5 移植 CV1000 生命周期与有序 worker 保护 |
| `9f952ac62` | 跨 CPU 行 worker 批量 CV1000 draw |
| `23c916ee2` | 移植有序 CV1000 blitter worker 与 CPU blend 快速路径 |
| `035759c35` | alpha 缓存里避开 SDK 的 page 宏 |
| `3dfde3aef` | Ibara 爆炸 blend 模式批量+向量化 |
| `5b627bcab`、`2711b2033` | 受保护的 SSE2 加法混合；简化 SIMD 饱和、批量写零 alpha |
| `17c80fbd7`、`ff51a4701` | 零 alpha 时跳过目标读；特化自身 alpha 混合 |
| `1155178d2` | 融合精确的固定 alpha 倒数乘法 |
| `eb195a61f` | **只对未命中的查询加载 alpha tile** |
| `e2389eb79` | 批量帧缓冲转换与跨行复制 |
| `e558864f5` | 移植行并行 PGM Psikyo / Seibu 渲染器 |
| `690a41c42` | 移植 System32 渲染与有界 GA2 轮询 |
| `4eee87050` | 移植音频 segment pitch 与静音声部快速路径 |

### 6.4 运行/构建基础设施

| 提交 | 内容 |
| --- | --- |
| `5411a5dff` | 初始化 SELF 构造函数、为 realloc 收缩拷贝加上界 |
| `1d0775035`、`c4c1b29e0` 等 | 小端动态核心定义、PIC 处理、lld 链接 |

---

## 7. 正确性与回归测试

`tests/` 下的目录就是保留下来的回归工具：

| 目录 | 用途 |
| --- | --- |
| `sh3_x64` | 把 native 块与解释器对跑；`run.py --sanitize` 上 ASan/UBSan |
| `sh3_threaded` | 直接线程化 dispatch 的等价性 |
| `sh3_opcode_dispatch` | 操作码 dispatch 重放对比 |
| `sh3_stopped_timer` | stopped timer 行为 |
| `console_replay` | 上面 5.1 的重放台 |
| `epic12_blit` / `epic12_screen_copy` / `epic12_cpu_batch` | 渲染 tile/blit 差分 |
| `system32_mixer` | 音频混合 |
| `z80_status_poll` / `v25_wait_loop` / `v60_wait_loop` | CPU 轮询循环 |
| `ps4_runtime` | PS4 运行时/生命周期 |

### 7.1 两个特别有用的技巧

**（a）native 执行计数器防假通过。**
`sh3_x64` 的测试会统计 native 与 fallback 的执行次数。如果每个指令都悄悄回退到解释器，
差分测试照样「通过」，但优化根本没生效。计数器让这种假通过无法发生。

**（b）头文件静态断言（fail-closed）。**
`tests/sh3_x64/run.py` 在建测试前先解析 `sh3_threaded.h` 的 `SH3_ALU_OPS` 列表，
回源码里找到每个 handler 的函数体，断言：

- 函数体里只出现 `m_r` / `m_sr`（即确实是纯 ALU），
- 不出现 `EAT`/`RB`/`RW`/`RL`/`WB`/`WW`/`WL`/`sh3_total_cycles`/`Sh3BurnCycles`。

一旦上游给某个「延迟槽 handler」加了副作用，断言立刻失败，而不是让 JIT 静默算错。

**（c）精确而不是近似的数学。**
alpha 相关候选（倒数、位移、packed add）都要求**完整数学证明 + 全量哈希**通过才继续测量，
不接受「视觉上看不出来」的近似。

### 7.2 Sanitizer 的已知边界

Clang sanitizer 构建会在 `byte_loop_cases` 上 SEGV，**HEAD 基线同样如此**：这是既有的未对齐取指 UB
（`sh3_cpu_readop16` 无边界检查），不是本轮引入的。因此没有把它设成门禁。详见第 9 节。

---

## 8. 实机调试技术（klog）

### 8.1 抓取

`testbuild/capture_klog_af.py`：连 PS4 klog 服务器（默认 `192.168.2.173:3232`），
长窗口（默认 10800 秒）持续写盘，并带**断线重连**——应用启动或崩溃本身会断开 socket，
没有重连就会漏掉崩溃转储。状态写 `<tag>-<stamp>.json`，正文写 `<tag>-<stamp>.log`。

三条必须知道的特性：

- **klog 只转发实时流，不重放历史缓冲。** 必须「先连接，再复现」，事后连上看不到任何东西。
- **同一服务器只允许一个连接**，第二个连接会把第一个踢掉（表现为 `peer_closed`）。
  所以要抓的时候先确认没有别的会话在连。
- 崩溃转储是内核在信号处理里同步打出来的，**不依赖应用存活**，所以第 1 帧就崩也能抓到。

### 8.2 分析

`testbuild/analyze_klog_crash.py`：

1. 从日志里切出 `# A user thread receives a fatal signal` 的 `#` 转储块（以 `<` 开头的 shell 行作为结束）。
2. 解析信号、`fault_address`、`rip` 和全部寄存器。
3. 用模块名匹配**本地同版本 ELF**，把 `rip` 归一到「模块相对偏移」。
4. 用 `nm -C --defined-only` 建符号表 + 二分查找，得到 `函数名 + 偏移`。
5. 落 `<log>-analysis.json`，可选 `--disassemble` 直接给崩溃指令附近的 4 条反汇编。

```
rtk python3 analyze_klog_crash.py <log> --output <json> --elf <core.elf> --elf <frontend.elf> --disassemble
```

**关键前提：ELF 必须和实机上跑的完全同版本。** 本项目的 AF 核心 ELF（`fbneo_libretro.elf`）
text 段 vaddr 从 `0x0` 起，所以「模块相对偏移」就等于 ELF 地址，符号化不需要重定位修正。

### 8.3 实战例子：ibara 金手指崩溃

- `SIGSEGV`，`fault_address = 0xffff`，`rip = 0x8006bc6c2` → 核心 `fbneo_libretro.self`，相对偏移 `0x2e06c2`
- 符号 `Sh3Run_threaded<true,true>(int,bool) + 0x31e2`
- 反汇编定位到取指路径：

```
2e06b0: movzwl %ax,%ecx          ; rcx = m_pc & 0xffff  (0xffff)
2e06b9: and    $0xfff8,%eax      ; (m_pc >> 16) * 8
2e06be: mov    (%rax,%r12,1),%rax ; MemMapF[m_pc>>16] = 0 (NULL)
2e06c2: movzwl (%rax,%rcx,1),%eax ; NULL + 0xffff → 段错误
```

结论用了三条独立证据，而不是「猜」：

1. `MemMapF` 表项为 NULL（该 64 KiB 页没有 FETCH 映射），代码里 `RB`/`WB` 都有 `>= SH3_MAXHANDLER` 守卫，
   **只有 `sh3_cpu_readop16` 没有**，所以这是取指 UB 的性质，不是随机内存损坏。
2. `m_pc & 0xffff == 0xffff` 是**奇数**，而 SH3 的 PC 第 0 位恒为 0 —— 说明 PC 已经被写坏，不是正常跳转。
3. 更早的另一版包（不含当时的 JIT 新提交）有**同类**崩溃（fault `0x10`，同样是 NULL 表项取指），
   所以这一类崩溃**先于**本轮 JIT 改动存在。

教训：**转储里的 `backtrace` 可能是空的**（本例就是），必须靠 rip + 寄存器 + 指令级反汇编来立论，
不能因为「没有调用栈」就下不了结论。

---

## 9. 已知问题与教训

- **未对齐取指 UB**：`sh3_cpu_readop16()` 对 `MemMapF` 无 NULL/handler 守卫，PC 跑飞即 SIGSEGV。
  HEAD 基线在 Clang sanitizer 下同样崩。
- **金手指与崩溃的因果未闭合**：cheat 写入走 `cheat_write → WB`（`MemMapW`），只写 guest RAM，
  不直接碰 `MemMapF` 或 `m_pc`；崩溃发生在 `retro_run call=1..3`，非常早。
  「是不是金手指触发」需要一次「关掉金手指同场景复现」的 A/B 才能定性。
- **`/data/retroarch/system/fbneo/cheats` 下没有 per-game ini 是正常的**：FBNeo 走 `cheat.dat`
  （`ConfigCheatLoad()` → `ConfigParseMAMEFile()`），libretro 侧再把它转成 core option。
- **宿主结果 ≠ 主机结果**：宿主 CPU/渲染线程模型与 PS4 不同，宿主 ±0.5% 一律视为噪声。
- **不要覆盖用户工作区**：参考仓库里常有未提交的优化和大量私有构建资料，
  不能用 `clean`/`reset`/`checkout` 去「弄干净」。
- **`objcopy --dump-section` 必须显式指定一次性输出文件**，否则会重写输入文件。

---

## 10. 工作纪律与本机约定

- **单一推送分支**：`git@github.com:liujiny/FBNeo.git`（remote `origin`）只保留
  `ps4-cv1k-demand-alpha-20261005` 一个 PS4/CV1000 工作分支。
  **以后一律直接推到该分支，不新开分支、不 force、不改 remote。**
- **不提交 ROM、SDK、密钥、大型日志或二进制**到仓库；ROM 只在测试目录里由用户提供。
- **诊断代码不进正式树**，实验分支可以多，正式分支要干净。
- **每轮留档**：`REPORT.md`（结论+证据）、`retained.json`（溯源+父提交+哈希）、
  以及 `testbuild/continuous-optimization-index.json`（全部轮次与状态的索引，防止重复试验）。
- 本机 shell 命令统一加 `rtk` 前缀（token 优化代理）；`rtk grep` 超过 25 行会截断，
  完整输出在 `~/.local/share/rtk/tee/*.log`。
- 交接上下文：`testbuild/continuous-optimization-context-20261005.md`；
  技能参考：`~/.codex/skills/retroarch-console-optimizations/references/ps4-current.md`。

---

## 11. 试过但未保留的方向

以下课题都实际做过并**未保留**，目录名即课题（详见 `testbuild/continuous-optimization-index.json`）：

- SH3/JIT：`sh3-call-terminal`、`sh3-call-delay`、`sh3-pr-threaded`、`sh3-ram-runs`、`sh3-codegen`、
  `sh3-simd-tail`、`sh3-imm8`、`sh3-regcache8`、`sh3-block-layout`、`sh3-lookup-reuse`、
  `sh3-nop-delay`、`sh3-div1`、`sh3-byte-loops`、`sh3-byte-outline`、`sh3-store-range`、
  `sh3-t-register`、`sh3-t-write-trigger`、`sh3-t-writeback`、`sh3-sr-value`、`sh3-sr-pair`
- CV1000/渲染：`cv1k-alpha512`、`cv1k-tint-recip`、`cv1k-packed-add`、`cv1k-tint-shift`、
  `cv1k-alpha-tiles`、`cv1k-alpha-halfrows`
- 已准备但未应用：`cv1k-alpha-querysimd`（JIT 预取候选因 Ibara P95 +7.43% 被否）

**动手前先查索引**，避免重复这些已经证伪的方向。

---

## 12. 复现 AF 终版

```
# 1) 取正式源码
git clone git@github.com:liujiny/FBNeo.git
git checkout ps4-cv1k-demand-alpha-20261005     # 11d50b202

# 2) 构建 PS4 动态核心（OpenOrbis + lld）
cd src/burner/libretro
make platform=orbis-dynamic clean
make platform=orbis-dynamic "CXX=clang++ -DFBNEO_SH3_X64_JIT=1" -j4 orbis-self

# 3) 回归（宿主，非 PS4 帧率）
cd ../../../
python3 tests/sh3_x64/run.py --sanitize --output /tmp/sh3-x64
python3 tests/console_replay/...                # 见 tests/console_replay/README.md

# 4) 组包：照 testbuild/af-terminal-branch-pkg-logs/ 的
#    prepare.py → compile.py → build-package.py → finalize-report.py 四步，
#    每步的产物与校验日志都留在该目录下。
```

验收清单（AF 已全部通过）：启动模拟器 → 加载 ROM → 画面正常 → R3 菜单正常 →
菜单缩略图正常 → 返回游戏正常 → Exit Game 正常。
