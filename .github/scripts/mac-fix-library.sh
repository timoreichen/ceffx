#!/bin/bash
# The build embeds the build machine's JDK as the library search path (LC_RPATH). On another machine, or with
# another JDK, that loads a second copy of AWT into the app and crashes it. Without it the library uses the JDK
# libraries the running app has already loaded. Same step as build-mac-popups.sh.
set -euo pipefail
LIB="$1"
for p in $(otool -l "$LIB" | awk '/LC_RPATH/{getline; getline; print $2}'); do
  echo "removing LC_RPATH $p"
  install_name_tool -delete_rpath "$p" "$LIB"
done
codesign -f -s - "$LIB"
otool -l "$LIB" | grep -c LC_RPATH && { echo "LC_RPATH still present"; exit 1; } || true
