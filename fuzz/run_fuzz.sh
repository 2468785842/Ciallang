#!/usr/bin/env bash
set -euo pipefail
name=${1:?usage: run_fuzz.sh lexer|parser [libFuzzer options]}
shift
case "$name" in lexer|parser) ;; *) exit 2 ;; esac
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
fuzz_build=${CIAL_FUZZ_BUILD:-"$repo/build/fuzz"}
mkdir -p "$fuzz_build/corpus/$name" "$fuzz_build/artifacts/$name"
# Keep generated coverage inputs and crash artifacts in the build directory.
for seed in "$repo"/tests/test_files/*.tjs "$repo"/tests/fuzz_regress/*.tjs; do
    [[ -f "$seed" ]] || continue
    destination="$fuzz_build/corpus/$name/$(basename -- "$seed")"
    [[ -e "$destination" ]] || head -c 4096 "$seed" > "$destination"
done
exec "$fuzz_build/fuzz/fuzz_$name" "$fuzz_build/corpus/$name" \
    -artifact_prefix="$fuzz_build/artifacts/$name/" \
    -max_total_time=60 -timeout=5 -rss_limit_mb=1024 -max_len=4096 \
    -dict="$repo/fuzz/tjs.dict" -print_final_stats=1 "$@"
