#!/usr/bin/env python3
"""Install the pinned official S31 SDK/tools in a workspace-local directory.

Run with Python 3.10+ (initially verified with Python 3.13). Downloads official
GitHub submodules and build tools, then PyPI packages into an isolated venv.
An existing SDK checkout must already match the pin; it is never reset.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys


def run(argv, **kwargs):
    subprocess.run([str(arg) for arg in argv], check=True, **kwargs)


def main():
    if sys.version_info < (3, 10):
        sys.exit("ESP-IDF requires Python 3.10 or later.")
    project = Path(__file__).resolve().parent.parent
    pin = json.loads((project / "config/idf-revision.json").read_text())
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools-root", type=Path, default=project.parent / "toolchains")
    args = parser.parse_args()
    root = args.tools_root.resolve()
    sdk = root / "esp-idf-s31"
    tools = root / "idf-tools-s31"
    root.mkdir(parents=True, exist_ok=True)
    if not sdk.exists():
        run(["git", "init", sdk])
        run(["git", "-C", sdk, "remote", "add", "origin", pin["repository"]])
        run(["git", "-C", sdk, "fetch", "--depth", "1", "origin", pin["revision"]])
        run(["git", "-C", sdk, "checkout", "--detach", "FETCH_HEAD"])
    revision = subprocess.check_output(["git", "-C", str(sdk), "rev-parse", "HEAD"],
                                       text=True).strip()
    if revision != pin["revision"]:
        sys.exit("Existing SDK does not match config/idf-revision.json; preserve it "
                 "and use --tools-root with another directory.")
    run(["git", "-C", sdk, "submodule", "update", "--init", "--recursive",
         "--depth", "1", "--jobs", "6"])
    env = dict(os.environ, IDF_PATH=str(sdk), IDF_TOOLS_PATH=str(tools),
               PIP_CACHE_DIR=str(tools / "pip-cache"))
    manager = sdk / "tools/idf_tools.py"
    run([sys.executable, manager, "install", "--targets=" + pin["target"]], env=env)
    run([sys.executable, manager, "install-python-env"], env=env)
    # idf_tools names its venv with SDK and Python major/minor versions.
    venv = tools / "python_env" / (
        f"idf6.1_py{sys.version_info.major}.{sys.version_info.minor}_env")
    run([venv / "bin/python", "-m", "pip", "install", "ninja==" + pin["ninja"]],
        env=env)
    print("SDK ready. Run scripts/idf.sh build (no hardware required).")


if __name__ == "__main__":
    main()
