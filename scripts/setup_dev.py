#!/usr/bin/env python3
"""ARIA DAW — Developer Environment Setup Script.

Installs all required dependencies for building ARIA DAW.
"""

import os
import platform
import subprocess
import sys

REQUIRED_TOOLS = {
    "cmake": "cmake --version",
    "git": "git --version",
    "python": "python --version",
    "pnpm": "pnpm --version",
    "cargo": "cargo --version",
}

OPTIONAL_TOOLS = {
    "ninja": "ninja --version",
    "clang-format": "clang-format --version",
    "clang-tidy": "clang-tidy --version",
}

def check_tool(name: str, command: str) -> bool:
    try:
        result = subprocess.run(command.split(), capture_output=True, text=True)
        if result.returncode == 0:
            print(f"  ✓ {name}: {result.stdout.strip().split(chr(10))[0]}")
            return True
    except FileNotFoundError:
        pass
    print(f"  ✗ {name}: NOT FOUND")
    return False

def main():
    system = platform.system()
    print(f"ARIA DAW — Developer Setup")
    print(f"Platform: {system} ({platform.machine()})")
    print()

    print("Checking required tools:")
    required_ok = all(check_tool(name, cmd) for name, cmd in REQUIRED_TOOLS.items())

    print()
    print("Checking optional tools:")
    for name, cmd in OPTIONAL_TOOLS.items():
        check_tool(name, cmd)

    print()
    if required_ok:
        print("✓ All required tools found.")
        print("Run 'cmake --preset debug' to configure the build.")
    else:
        print("✗ Some required tools are missing. Install them and re-run.")
        sys.exit(1)

if __name__ == "__main__":
    main()
