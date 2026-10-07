#!/usr/bin/env bash
# One-shot status of the PS4 FBNeo CV1000 optimization project.
#
#   bash scripts/ps4_fbneo_status.sh
#
# Read-only.  Prints: source checkout state, the optimization index summary,
# and the packages currently in out/.  Run this before assuming any HEAD,
# round or package written in the skill references is still current.
set -uo pipefail

ROOT="${ROOT:-/mnt/e/Projects/retroarch-ps4-upstream-sync}"
SRC="$ROOT/testbuild/fbneo-sh3-jit"
IDX="$ROOT/testbuild/continuous-optimization-index.json"
OUT="$ROOT/dist/retroarch-ps4-upstream/out"

echo "=== source: $SRC ==="
if git -C "$SRC" rev-parse --git-dir >/dev/null 2>&1; then
  git -C "$SRC" log -1 --format='HEAD    %H%nsubject %s%ndate    %ci'
  echo "branch  $(git -C "$SRC" rev-parse --abbrev-ref HEAD)"
  dirty=$(git -C "$SRC" status --porcelain | wc -l)
  if [ "$dirty" -gt 0 ]; then
    echo "dirty   $dirty file(s)   <-- working tree NOT clean"
  else
    echo "dirty   0 (clean)"
  fi
  echo "push to $(git -C "$SRC" remote get-url origin)"
else
  echo "MISSING or not a git checkout"
fi

echo
echo "=== index: $IDX ==="
if [ -f "$IDX" ]; then
  python3 - "$IDX" <<'INDEX_EOF'
import json, sys, collections
d = json.load(open(sys.argv[1]))
print("formal_commit :", d.get("formal_commit"))
print("latest_pkg    :", d.get("latest_pkg"))
print("current       :", d.get("current"))
pk = d.get("packaging") or {}
print("packaging     :", pk.get("status"), pk.get("title_id"), pk.get("pkg"))
if pk.get("sha256"):
    print("pkg sha256    :", pk["sha256"], pk.get("bytes"), "bytes")
print("pkg source    :", pk.get("source_commit"))
rounds = d.get("rounds") or []
c = collections.Counter(r.get("status", "?") for r in rounds)
print("rounds        : %d total  %s" % (len(rounds), dict(c)))
ret = [r["directory"] for r in rounds if r.get("status") == "retained"]
if ret:
    print("retained dirs :", ", ".join(ret))
INDEX_EOF
else
  echo "MISSING"
fi

echo
echo "=== packages in out/ ==="
found=0
for f in "$OUT"/*.pkg; do
  [ -f "$f" ] || continue
  found=1
  printf '%12s  %s\n' "$(stat -c%s "$f")" "$(basename "$f")"
done
[ "$found" -eq 0 ] && echo "(none)"

echo
echo "Reminder: host replay results are NOT PS4 frame rates; only user hardware"
echo "confirmation counts as a performance result."
