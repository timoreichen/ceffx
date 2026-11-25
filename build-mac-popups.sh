#!/bin/zsh
# CEFFX for Rocketbatch: upstream bc3b5a2 (the build behind ceffx-natives 1.0.0-20260920) plus real popups in
# off-screen rendering (window.open with window features: sign-in windows of Google, Apple, payments).
# Builds the macOS arm64 natives, makes the library loadable on any machine and installs ceffx-core,
# ceffx-natives and ceffx-runtime into ~/.m2.
#
# STOP THE ROCKETBATCH CLIENT FIRST: replacing the jars under a running client breaks its later class loads.
# On the next start the client's CefRuntime replaces the libceffx.dylib of the installed runtime by itself.
set -e
ROOT="${0:A:h}"
export JAVA_HOME="${JAVA_HOME:-/Library/Java/JavaVirtualMachines/liberica-jdk-25.jdk/Contents/Home}"

cd "$ROOT/native/build"
make -j8 ceffx

# The build embeds this machine's JDK as the library search path (LC_RPATH). On another machine, or with
# another JDK, that loads a second copy of AWT into the app and crashes it. Without it the library uses the
# JDK libraries the running app has already loaded, as the upstream CI build does.
LIB=Release/mac-aarch64/libceffx.dylib
for p in $(otool -l "$LIB" | awk '/LC_RPATH/{getline; getline; print $2}'); do
  install_name_tool -delete_rpath "$p" "$LIB"
done
codesign -f -s - "$LIB"

cd "$ROOT/java"
mvn -q -DskipTests -pl ceffx-core,ceffx-natives install

cd "$HOME/Documents/Development/jfxcef-demo/ceffx-runtime"
mvn -q -DskipTests install

echo "CEFFX with popups installed. Start the client: its runtime takes the new library at the start."
