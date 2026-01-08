#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 <output_dir> [build_dir] [duration_seconds]" >&2
}

if [ "$#" -lt 1 ] || [ "$#" -gt 3 ]; then
  usage
  exit 1
fi

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "$script_dir/../.." && pwd)

output_dir=$1
build_dir=${2:-build}
duration_seconds=${3:-}

if [[ "$output_dir" = /* ]]; then
  output_dir_abs=$output_dir
else
  output_dir_abs="$repo_root/$output_dir"
fi

if [[ "$build_dir" = /* ]]; then
  build_dir_abs=$build_dir
else
  build_dir_abs="$repo_root/$build_dir"
fi

cmake -S "$repo_root" -B "$build_dir_abs"
cmake --build "$build_dir_abs"

pushd "$repo_root/cli/examples" >/dev/null
npm install
npm run build
popd >/dev/null

node "$repo_root/cli/scripts/bounce-test-graphs.js" \
  "$output_dir_abs" \
  "$build_dir_abs/cli/elemoffline" \
  "8"
