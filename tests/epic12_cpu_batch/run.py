#!/usr/bin/env python3
"""Build/run CPU batch rasterizer differential tests; no ROMs or PS4 SDK needed."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="keep executable and result.log here")
    parser.add_argument("--sanitizer", choices=("address,undefined", "thread"))
    parser.add_argument("--scalar", action="store_true", help="disable explicit SSE2 paths")
    parser.add_argument("--timeout", type=int, default=180,
                        help="maximum test execution seconds (default: 180)")
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    src = here.parent.parent / "src"
    includes = [src / p for p in ("burn", "cpu", "burn/snd", "burn/devices", "burner",
                                  "cpu/sh4", "burner/libretro",
                                  "burner/libretro/libretro-common/include")]
    with tempfile.TemporaryDirectory(prefix="fbneo-cpu-batch-") as temp:
        out = args.output.resolve() if args.output else Path(temp)
        out.mkdir(parents=True, exist_ok=True)
        binary = out / "epic12-cpu-batch-test"
        command = shlex.split(os.environ.get("CXX", "g++")) + [
            "-std=gnu++98", "-O2", "-g", "-pthread", "-D__LIBRETRO__",
            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-Wno-write-strings",
        ]
        if args.sanitizer:
            command += ["-fsanitize=" + args.sanitizer, "-fno-omit-frame-pointer"]
        if args.scalar:
            command += ["-U__SSE2__"]
        command += ["-I" + str(p) for p in includes]
        command += [str(here / "test.cpp"), "-o", str(binary)]
        print(shlex.join(command), flush=True)
        subprocess.run(command, cwd=out, check=True)
        result = subprocess.run([str(binary)], cwd=out, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=args.timeout)
        (out / "result.log").write_text(result.stdout)
        print(result.stdout, end="")
        if result.returncode:
            raise SystemExit(result.returncode)
        if args.output:
            print("Artifacts:", out)


if __name__ == "__main__":
    main()
