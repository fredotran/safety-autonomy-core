#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"

echo "[policy] Running banned API checks"

# Disallow exception throw/catch usage in core safety code.
if grep -R -n -E '\bthrow\b|\bcatch\b' include src --include='*.hpp' --include='*.cpp'; then
    echo "[FAIL] Exceptions are not allowed in core safety code" >&2
    exit 1
fi

# Disallow direct dynamic allocation calls in core safety code.
if grep -R -n -E '\bnew\b|\bdelete\b|\bmalloc\b|\bcalloc\b|\brealloc\b' include src --include='*.hpp' --include='*.cpp'; then
    echo "[FAIL] Dynamic allocation APIs are not allowed in core safety code" >&2
    exit 1
fi

# Disallow direct process aborts in core paths.
if grep -R -n -E '\babort\s*\(' include src --include='*.hpp' --include='*.cpp'; then
    echo "[FAIL] abort() usage is not allowed in core safety code" >&2
    exit 1
fi

echo "[policy] OK"
