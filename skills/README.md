# Codex skills for this repository

`ps4-fbneo-cv1000-optimization/` is a [Codex skill](https://github.com/openai/codex) —
a reusable set of instructions the agent loads automatically when a task matches its
description.

It covers continuing the CV1000 (Cave SH3) work on PS4: locating frame drops in
ddpsdoj / ddpdfk / ibara, SH3 JIT and epic12 renderer changes, host-replay
regression, and the OpenOrbis PKG pipeline.

The methodology itself lives in `docs/PS4_CV1000_OPTIMIZATION_PLAYBOOK.md`; the skill
routes to it rather than duplicating it.

## Install

```bash
cp -r skills/ps4-fbneo-cv1000-optimization ~/.codex/skills/
```

Then ask for the work in plain language, or invoke it explicitly with
`$ps4-fbneo-cv1000-optimization`.

Quick status check (read-only):

```bash
bash ~/.codex/skills/ps4-fbneo-cv1000-optimization/scripts/ps4_fbneo_status.sh
```
