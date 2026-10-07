# PKG 打包

流程全文见源码仓库 `docs/PS4_CV1000_OPTIMIZATION_PLAYBOOK.md` §3 / §4。
最完整的可跑模板是 `testbuild/ai-arena64m-pkg-logs/`（最新一轮，四阶段脚本齐全）。
**照抄该目录的四件套，不要从零写。**

## 目录形态

```
testbuild/<round>-pkg-logs/
  prepare.py  compile.py  build-package.py  finalize-report.py
```

每阶段失败即断言退出，产物与校验日志全部留在该目录下。

| 脚本 | 干什么 |
|---|---|
| `prepare.py` | 从**已验证的上一轮 stage** 复制出本轮 stage（`assert not stage.exists()`）；用结构体解析改写 `sce_sys/param.sfo` 的 `TITLE`/`TITLE_ID`/`CONTENT_ID`；同步改 `Project.gp4` 里的 CONTENT_ID；复制已验证前端并**逐个核对前端 payload 哈希**；落 `source-commit.txt`；拉起打包容器 |
| `compile.py` | 断言工作区干净、HEAD 等于 `source-commit.txt`；记录 `sh3_x64_jit.h` 哈希；容器内 clean 构建，落 `fbneo-clean-build.log` |
| `build-package.py` | 把 `fbneo_libretro.self` 装进 `stage/cores/`，做各项审计，再 PKG 生成 + 校验 + 抽取比对 |
| `finalize-report.py` | 汇总 `build_report.json`，写入 ELF 符号表、各段哈希、溯源，并生成 `<ROUND>_README.txt` |

## 打包容器

```python
subprocess.run(['docker','run','-d','--name','retroarch-ps4-<round>-pkg','-v',str(p)+':/project',
                '-e','DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1','ubuntu:20.04','sleep','infinity'])
# 然后
docker exec retroarch-ps4-<round>-pkg sh -lc \
  'apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq libssl1.1 libicu66'
```

* 容器名带轮次（`retroarch-ps4-ai-pkg` 等）。名字已存在就先复用或改名，别硬覆盖。
* PkgTool 在**挂载进容器的**工程里：`/project/toolchains/openorbis/bin/linux/PkgTool.Core`，
  用 `docker exec --workdir <stage> <container> PkgTool.Core ...` 调用。
* 用的镜像哈希记在 `build-provenance.json` 的 `image` 字段
  （AI 轮是 `sha256:5eba9d21c15b06f2885f3834657ee229f0c0a6e9180ed1d36c6e96ce17b85fb1`）。
* 容器可能长时间挂在后台。清理前先确认没有正在跑的轮次。

## stage 复用规则

**不重新制备 stage。** 复制上一轮已验证 stage，只改三处：

1. `sce_sys/param.sfo` 的 `TITLE` / `TITLE_ID` / `CONTENT_ID`（用结构体解析，不是字符串替换）；
2. `Project.gp4` 里的 `CONTENT_ID`；
3. `cores/fbneo_libretro.self` 换成这一轮构建的产物。

前端（`retroarch_orbis.*`）沿用已验收的那份，**字节一致**。改核心时不动前端能大幅缩小变量。

## PkgTool 调用序列

```
pkg_build        Project.gp4  <outdir>            -> pkg-build.log
pkg_validate --verbose <pkg>                      -> pkg-validate.log（必须 32 项全 [OK]）
pkg_listentries  <pkg>                            -> pkg-listentries.log
pkg_extract      --passcode <32 个 0> <pkg> <dir> -> pkg-extract.log
pkg_extractentry <pkg> <index> <dir>              -> extract-entry-<index>.log
```

## 打包前的验证门（缺一项就不打）

| 证据文件 | 要求 |
|---|---|
| `object-module-audit.json` | 全部 1140 个对象是 FreeBSD AMD64，`non_FreeBSD_objects` 为空 |
| `import-audit.json` | 未定义符号集合与上一版比对（`undefined_symbols_equal_AB`），不许悄悄引入新依赖 |
| `dynamic-audit.txt` | `NEEDED` 只允许 `libkernel.so` / `libSceLibcInternal.so`；检查 PIE、RELA/RELACOUNT |
| `frontend-heap-audit.log` | 前端 elf/oelf/self 三段都 `ordinary/internal heap=256MiB`，其余 CRT 字段不变 |
| `packaged-param.sfo` | 改写后复核语义，不只看字节写成功 |
| `pkg-validate.log` | 32 项全 `[OK]` |
| payload 哈希 | 包内每个 payload 逐一比对来源哈希（18 项），`finalize-report.py` 会 assert |
| 溯源交叉验证 | 用**目标 `.o` 的 SHA** 证明改动真的进了包 |

> 只比对最终 `.self` 哈希是不够的 —— 它同时受链接顺序影响。
> 用**单个 `.o`** 的哈希变化证明"哪个源文件进了产物"最可靠。
> 例：AI 从 AF 出发，`sh4.o` 由 `5821b402…` 变 `10733f53…`（证明 JIT 头进了 CPU 对象），
> renderer 的 `epic12.o` 仍是 `a9652a2c…`（证明没被牵连）。

## 产物与命名

`dist/retroarch-ps4-upstream/out/` 下每轮四件套加包本体：

```
<ROUND>_README.txt          <ROUND>_SHA256SUMS.txt
<ROUND>_build_report.json   <ROUND>_RELEASE_NOTES.md
RetroArch_PS4_<ROUND>_<描述>_<TITLEID>.pkg
```

Title ID 递增：AF = `RAPS10029`，AI = `RAPS10030`。
Content ID 形如 `UP0001-RAPS10030_00-0000000000000001`。

`<ROUND>_README.txt` 里必须写清：Title/Content ID、前端提交、FBNeo 提交、编译 flags、
字节数、SHA256、验证统计，以及「**PS4 硬件性能待测，宿主结果不代表 PS4 帧率**」。

## 打包后

1. 把 `packaging` 段写进 `testbuild/continuous-optimization-index.json`
   （`title_id` / `content_id` / `pkg` / `sha256` / `bytes` / `source_commit` / `report`），
   `latest_pkg` 指向本轮，删掉旧的 `pending_pkg`。
2. 状态先记 `complete_hardware_pending`，等用户实机确认后再改。
3. 提交源码（不带包）并推 `ps4-cv1k-demand-alpha-20261005`。包本身留在 `out/`，不进 Git。
