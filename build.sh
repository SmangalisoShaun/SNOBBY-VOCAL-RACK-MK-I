#!/usr/bin/env bash
# SNOBBY VOCAL RACK MK I - one script: installs tools, gets JUCE, builds the VST3.
# Run from the repo root:   chmod +x build.sh && ./build.sh
set -e
cd "$(dirname "$0")"

# 1. build tools (Codespaces / Ubuntu)
if command -v apt-get >/dev/null 2>&1; then
  SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"
  $SUDO apt-get update -y
  $SUDO apt-get install -y cmake ninja-build pkg-config git libasound2-dev libx11-dev libxcomposite-dev \
    libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libfreetype-dev \
    libfontconfig1-dev libgl1-mesa-dev
fi

# 2. JUCE (pinned to 8.0.4)
if [ ! -f JUCE/CMakeLists.txt ]; then
  rm -rf JUCE
  git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
fi

# 3. let VS Code find the JUCE headers (removes the red include squiggles)
mkdir -p .vscode
cat > .vscode/c_cpp_properties.json <<'EOF'
{
  "configurations": [
    { "name": "Linux", "compileCommands": "${workspaceFolder}/build/compile_commands.json", "cppStandard": "c++17" }
  ],
  "version": 4
}
EOF

# 4. configure + build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --target BoomhypeVocalRack_VST3 -j"$(nproc)"

echo
echo "DONE. Linux VST3 is in: build/BoomhypeVocalRack_artefacts/Release/VST3/"
echo "For FL Studio on Windows: git add . && git commit -m build && git push, then download the VST3 from the GitHub Actions tab."
