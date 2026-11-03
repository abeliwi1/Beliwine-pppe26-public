#!/bin/sh
# setup_linux.sh -- make OpenCilk's compiler build Activity 4 on Linux (x86-64).
#
#   ./setup_linux.sh            # OpenCilk unpacked at ~/opencilk
#   OPENCILK=/path ./setup_linux.sh
#
# OpenCilk ships a clang but no OpenMP runtime, and Activity 4 needs both.
# The runtime must be at least as new as OpenCilk's clang: an older one, such
# as Ubuntu 24.04's libomp-dev (LLVM 18), fails to link schedule(dynamic) with
# "undefined reference to __kmpc_dispatch_deinit".  This script:
#
#   1. builds the OpenMP runtime from the LLVM release that matches OpenCilk's
#      clang, and installs omp.h and libomp.so into OpenCilk itself;
#   2. writes clang.cfg and clang++.cfg next to OpenCilk's clang, so that every
#      build finds libomp.so, and, if needed, a GCC with C++ headers;
#   3. builds and runs a small Cilk + OpenMP test.
#
# Needs curl, gcc/g++, and cmake (sudo apt install cmake), or python3 with
# venv, from which it installs cmake in a temporary directory.  No sudo.
set -e

OPENCILK=${OPENCILK:-$HOME/opencilk}
CLANG=$OPENCILK/bin/clang
[ -x "$CLANG" ] || { echo "no OpenCilk clang at $CLANG (set OPENCILK)" >&2; exit 1; }

VERSION=$("$CLANG" --version | sed -n 's/.*clang version \([0-9][0-9.]*\).*/\1/p')
RESOURCE=$("$CLANG" -print-resource-dir)
echo "OpenCilk at $OPENCILK: clang $VERSION"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# --- 1. The C++ standard library headers --------------------------------------
# Clang picks the newest GCC it finds, even if only GCC's runtime is installed
# and not its C++ headers (e.g. Ubuntu 24.04 with GCC 14's runtime but only
# g++-13).  If <algorithm> doesn't compile, pick the newest GCC that has them.
# --no-default-config ignores any clang++.cfg from an earlier run of this script.
GCC_FLAG=
printf '#include <algorithm>\nint main(){}\n' > "$WORK/t.cpp"
if ! "$OPENCILK/bin/clang++" --no-default-config -c "$WORK/t.cpp" -o "$WORK/t.o" 2>/dev/null; then
    for dir in $(ls -d /usr/lib/gcc/x86_64-linux-gnu/* | sort -V -r); do
        if [ -d "/usr/include/c++/$(basename "$dir")" ] &&
           "$OPENCILK/bin/clang++" --no-default-config --gcc-install-dir="$dir" -c "$WORK/t.cpp" -o "$WORK/t.o" 2>/dev/null; then
            GCC_FLAG="--gcc-install-dir=$dir"
            break
        fi
    done
    [ -n "$GCC_FLAG" ] || { echo "no GCC with C++ headers found: sudo apt install g++" >&2; exit 1; }
    echo "C++ headers: using $GCC_FLAG"
fi

# --- 2. The OpenMP runtime ------------------------------------------------------
if [ -f "$OPENCILK/lib/libomp.so" ] && [ -f "$RESOURCE/include/omp.h" ] &&
   nm -D "$OPENCILK/lib/libomp.so" | grep -q __kmpc_dispatch_deinit; then
    echo "OpenMP runtime: already installed"
else
    echo "OpenMP runtime: building LLVM $VERSION's (a few minutes)"
    URL=https://github.com/llvm/llvm-project/releases/download/llvmorg-$VERSION
    for part in openmp cmake; do
        curl -fsSL "$URL/$part-$VERSION.src.tar.xz" | tar xJ -C "$WORK"
        mv "$WORK/$part-$VERSION.src" "$WORK/$part"     # openmp expects ../cmake
    done

    CMAKE=$(command -v cmake || true)
    if [ -z "$CMAKE" ]; then
        python3 -m venv "$WORK/venv" ||
            { echo "need cmake: sudo apt install cmake" >&2; exit 1; }
        "$WORK/venv/bin/pip" -q --disable-pip-version-check install cmake
        CMAKE=$WORK/venv/bin/cmake
    fi

    "$CMAKE" -S "$WORK/openmp" -B "$WORK/build" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_INSTALL_PREFIX="$WORK/install" \
        -DOPENMP_ENABLE_LIBOMPTARGET=OFF -DLIBOMP_OMPD_SUPPORT=OFF \
        -DOPENMP_ENABLE_OMPT_TOOLS=OFF -DLIBOMP_INSTALL_ALIASES=OFF > "$WORK/cmake.log" 2>&1 ||
        { cat "$WORK/cmake.log" >&2; exit 1; }
    "$CMAKE" --build "$WORK/build" --target install -j "$(nproc)" > "$WORK/build.log" 2>&1 ||
        { tail -30 "$WORK/build.log" >&2; exit 1; }

    cp "$WORK"/install/include/*.h "$RESOURCE/include/"
    cp "$WORK/install/lib/libomp.so" "$OPENCILK/lib/"
    echo "OpenMP runtime: installed omp.h in $RESOURCE/include, libomp.so in $OPENCILK/lib"
fi

# --- 3. Config files --------------------------------------------------------------
# Clang reads <name>.cfg from its own directory on every run.  Config files
# don't expand variables, so the paths are written out in full.
for name in clang clang++; do
    cfg=$OPENCILK/bin/$name.cfg
    # Keep a config this script didn't write, once, as <name>.cfg.orig.
    if [ -f "$cfg" ] && ! grep -q "setup_linux.sh" "$cfg" && [ ! -f "$cfg.orig" ]; then
        cp "$cfg" "$cfg.orig"
    fi
    {
        echo "# Written by Activity 4's setup_linux.sh."
        echo "# libomp.so: -L finds it at link time and the rpath at run time."
        [ -n "$GCC_FLAG" ] && echo "$GCC_FLAG"
        echo "-L$OPENCILK/lib"
        echo "-Wl,-rpath,$OPENCILK/lib"
    } > "$cfg"
done
echo "wrote $OPENCILK/bin/clang.cfg and clang++.cfg"

# --- 4. Test: Cilk and OpenMP, including schedule(dynamic) -------------------------
cat > "$WORK/test.cpp" <<'EOF'
#include <algorithm>
#include <cstdio>
#include <vector>
#include <cilk/cilk.h>
#include <omp.h>
int main() {
    std::vector<long> a(1 << 20, 1);
    long s = 0;
    #pragma omp parallel for schedule(dynamic, 1024) reduction(+:s)
    for (long i = 0; i < (long)a.size(); i++) s += a[i];
    cilk_for (long i = 0; i < (long)a.size(); i++) a[i] = 2;
    long t = std::count(a.begin(), a.end(), 2);
    printf("OpenMP sum %ld, Cilk count %ld, %d OpenMP threads\n", s, t, omp_get_max_threads());
    return !(s == (1 << 20) && t == (1 << 20));
}
EOF
"$OPENCILK/bin/clang++" -fopencilk -fopenmp -O2 -std=c++17 "$WORK/test.cpp" -o "$WORK/test"
"$WORK/test"
echo "setup ok: build Activity 4 with the handout's Linux command"
