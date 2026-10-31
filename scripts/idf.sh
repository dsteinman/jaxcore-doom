#!/usr/bin/env bash
# Build the pinned S31 project without modifying a shell's persistent setup.
set -euo pipefail
project_root=$(cd "$(dirname "$0")/.." && pwd)
tools_root=${JAXCORE_TOOLS_ROOT:-"$project_root/../toolchains"}
if [[ ! -d "$tools_root/esp-idf-s31" ]]; then
    echo "Run Python 3.10+ scripts/setup_idf.py first." >&2
    exit 1
fi
tools_root=$(cd "$tools_root" && pwd)
sdk_path="$tools_root/esp-idf-s31"
export IDF_TOOLS_PATH="$tools_root/idf-tools-s31"
export IDF_COMPONENT_CACHE_PATH="$IDF_TOOLS_PATH/component-cache"
if [[ ! -f "$sdk_path/export.sh" ]]; then
    echo "Run Python 3.10+ scripts/setup_idf.py first." >&2
    exit 1
fi
if [[ -z ${IDF_PYTHON_ENV_PATH:-} ]]; then
    environments=("$IDF_TOOLS_PATH"/python_env/idf6.1_py*_env)
    if [[ ${#environments[@]} != 1 || ! -x "${environments[0]}/bin/python" ]]; then
        echo "Set IDF_PYTHON_ENV_PATH to the desired installed IDF venv." >&2
        exit 1
    fi
    export IDF_PYTHON_ENV_PATH="${environments[0]}"
fi
export PATH="$IDF_PYTHON_ENV_PATH/bin:$PATH"
expected_revision=$(python -c 'import json,sys; print(json.load(open(sys.argv[1]))["revision"])' \
    "$project_root/config/idf-revision.json")
actual_revision=$(git -C "$sdk_path" rev-parse HEAD)
if [[ "$actual_revision" != "$expected_revision" ]]; then
    echo "ESP-IDF checkout differs from config/idf-revision.json." >&2
    exit 1
fi
# export.sh performs the SDK's tool/version/dependency checks.
source "$sdk_path/export.sh" >/dev/null
cd "$project_root"
exec python "$sdk_path/tools/idf.py" -B "$project_root/build/s31" \
    -D "SDKCONFIG=$project_root/build/s31/sdkconfig" \
    -D "SDKCONFIG_DEFAULTS=$project_root/sdkconfig.defaults" "$@"
