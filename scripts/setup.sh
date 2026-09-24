#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
project_root=$(cd "$script_dir/.." && pwd)
gem5_root=${GEM5_ROOT:-$project_root/gem5}
gem5_repo=${GEM5_REPO:-https://github.com/gem5/gem5.git}
gem5_commit=${GEM5_COMMIT:-af72b9ba580546ac12ce05bfaac3fd53fa8699f4}

required_commands=(git)
if [[ ${SKIP_BUILD:-0} != 1 ]]; then
    required_commands+=(scons)
fi
for command in "${required_commands[@]}"; do
    command -v "$command" >/dev/null || {
        echo "Required command not found: $command" >&2
        exit 2
    }
done

if [[ ! -d "$gem5_root/.git" ]]; then
    [[ ! -e "$gem5_root" ]] || {
        echo "Refusing to overwrite non-git path: $gem5_root" >&2
        exit 2
    }
    git clone --filter=blob:none --no-checkout "$gem5_repo" "$gem5_root"
elif [[ -n $(git -C "$gem5_root" status --porcelain) ]]; then
    echo "Refusing to replace files in a dirty gem5 checkout: $gem5_root" >&2
    echo "Commit/stash its changes or choose a new GEM5_ROOT." >&2
    exit 2
fi

git -C "$gem5_root" fetch origin "$gem5_commit"
git -C "$gem5_root" checkout --detach "$gem5_commit"
GEM5_ROOT="$gem5_root" "$script_dir/apply_overlay.sh"

if [[ ${SKIP_BUILD:-0} != 1 ]]; then
    GEM5_ROOT="$gem5_root" "$script_dir/build.sh"
fi
