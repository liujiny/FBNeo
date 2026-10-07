# 当前状态

## 源码

| 项 | 值 |
|---|---|
| 检出 | `/mnt/e/Projects/retroarch-ps4-upstream-sync/testbuild/fbneo-sh3-jit` |
| 分支 | `ps4-cv1k-demand-alpha-20261005` |
| HEAD | `d1a75056d9fd297582fe10849a0c282149ce7d0d` |
| origin | `git@github.com:liujiny/FBNeo.git` |
| 其他 remote | `libretro`、`upstream`（只读参考，不推） |

> 本文件的状态会过期。**先跑 `scripts/ps4_fbneo_status.sh` 或读索引确认当前实际值**，
> 不要把这里的 HEAD 当成今天的事实。

## 索引与上下文

| 文件 | 用途 |
|---|---|
| `testbuild/continuous-optimization-index.json` | 全部轮次与状态、`latest_pkg`、`packaging`。**动手前先查 `rounds`** |
| `testbuild/continuous-optimization-context-20261005.md` | 交接上下文（37KB，较长） |
| `testbuild/ai-arena64m-pkg-logs/` | 最新一轮打包的完整证据目录 |
| `~/.codex/skills/retroarch-console-optimizations/references/ps4-current.md` | PS4 现状总档（95KB），跨主机历史也在这个技能里 |

索引里每条 `rounds[].status` 取值有 `retained` / `not retained` / `diagnostic` 等，
含义是"这一轮的方向保没保留"，不是"跑没跑过"。

## 最新成品包

| 项 | 值 |
|---|---|
| 包 | `RetroArch_PS4_AI_Arena64M_SH3_RAPS10030.pkg` |
| 目录 | `dist/retroarch-ps4-upstream/out/` |
| 大小 / SHA256 | 179306496 字节 / `908e1a195aafd3c771d103cfd821aa062d1bc482a0ea0cdf6532669ef820ce68` |
| Title ID | `RAPS10030` |
| 源码 | `d1a75056d9fd297582fe10849a0c282149ce7d0d` |
| 索引状态 | `complete_hardware_pending` —— 包好了，**等实机确认** |

上一版是 AF `RAPS10029`（`1beb1515…`，提交 `11d50b202`），其内容已由用户实机确认
ddpsdoj / ddpdfk 基本满帧。AI 是在 AF 之上把 SH3 代码竞技场从 8MiB 扩到 64MiB。

`out/` 里每轮的产物是四件套：`<ROUND>_README.txt`、`<ROUND>_SHA256SUMS.txt`、
`<ROUND>_build_report.json`、`<ROUND>_RELEASE_NOTES.md`，加上 `.pkg` 本体。

## 仓库内文档

`docs/PS4_*.md` 是逐个课题的独立记录（`PS4_CV1000_OPTIMIZATION_PLAYBOOK.md` 是总纲，
其余按 JIT / 渲染 / 打包 / 调试分主题）。要查某个已做过课题的细节，直接读对应那一篇，
不要从索引和上下文里重新推断。

## 测试目录

| 目录 | 用途 |
|---|---|
| `tests/sh3_x64/` | SH3→x64 JIT 正确性回归，`python3 run.py`，加 `--sanitize` 走 Clang sanitizer |
| `tests/console_replay/` | 宿主演进重放与差分（**不是 PS4 帧率**），见其 `README.md` |
| `testbuild/epic12_*`、`testbuild/v60_wait_loop` 等 | 各子系统的独立测试夹具 |

用户自己的状态快照（`ddpsdoj.state`、`ddpdfk.state`）和 ROM 放在
`dist/retroarch-ps4-upstream/out/`，**不要提交进仓库**。
