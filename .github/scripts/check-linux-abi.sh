#!/usr/bin/env bash
# Keep the executable within sniper's GLIBC 2.31 floor and bundle the GCC runtimes.
set -euo pipefail

bin=${1:?Usage: check-linux-abi.sh executable}
needed=$(objdump -p "$bin" | grep NEEDED)
printf '%s\n' "$needed"
newest=$(objdump -T "$bin" | grep -oE 'GLIBC_[0-9]+(\.[0-9]+)+' | sort -uV | tail -n1)
echo "Newest glibc symbol version: $newest"
if [ "$(printf '%s\n' "$newest" GLIBC_2.31 | sort -V | tail -n1)" != GLIBC_2.31 ]; then
    echo "::error::$bin needs $newest; the Steam Runtime 3 floor is GLIBC_2.31"
    exit 1
fi
if grep -qE 'NEEDED +lib(stdc\+\+|gcc_s)' <<<"$needed"; then
    echo "::error::$bin must link libstdc++ and libgcc statically"
    exit 1
fi
