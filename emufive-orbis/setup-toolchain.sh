#!/usr/bin/env bash
# Monta um toolchain C++20 para PS4 (Orbis) a partir do código-fonte, sem usar os releases do OpenOrbis.
# Foi o que usei aqui. Precisa de: clang 18, lld, cmake, ninja, go, python3, git, make.
set -euo pipefail
W=${1:-$PWD/oo}; mkdir -p "$W"; cd "$W"
git clone --depth 1 https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain.git tc
git clone --depth 1 https://github.com/OpenOrbis/musl.git musl
git clone --depth 1 https://github.com/OpenOrbis/create-fself.git create-fself
git clone --depth 1 https://github.com/OpenOrbis/orbis-lib-gen.git orbis-lib-gen
git clone --depth 1 --branch 5.05 https://github.com/idc/ps4libdoc.git orbis-lib-gen/ps4libdoc
git clone --depth 1 --branch llvmorg-18.1.3 --filter=blob:none --sparse https://github.com/llvm/llvm-project.git llvm18
(cd llvm18 && git sparse-checkout set runtimes libcxx libcxxabi libunwind cmake llvm/cmake third-party compiler-rt)

# 1) libc (musl do OpenOrbis)
(cd musl && mkdir -p ps4 && ./configure --target=x86_64-scei-ps4 --disable-shared CC=clang \
   CFLAGS="-fPIC -DPS4 -D_LIBUNWIND_IS_BAREMETAL=1" --prefix=./ps4 && make -j"$(nproc)" && make install)
# O math.h do OpenOrbis declara sobrecargas C++ que brigam com a libc++: desliga quando a libc++ está presente.
sed -i '68s/.*/#if defined(__cplusplus) \&\& !defined(_LIBCPP_VERSION)/;192s/.*/#if defined(__cplusplus) \&\& !defined(_LIBCPP_VERSION)/' musl/ps4/include/math.h

# 2) stubs das bibliotecas do sistema + create-fself
(cd orbis-lib-gen && python3 generate.py ps4libdoc/system/common/lib && python3 gen_makefile.py && make -C build -j"$(nproc)")
(cd create-fself/cmd/create-fself && go build -o "$W/create-fself" -modfile=go-linux.mod)

# 3) libc++ 18 (o libc++ do OpenOrbis é antigo e não tem ranges/jthread)
mkdir -p shim && printf '#include <locale.h>\n' > shim/xlocale.h
rm -rf inc-cxx && cp -r musl/ps4/include inc-cxx && rm inc-cxx/sys/sendfile.h
F="-fPIC -DPS4 -D_LIBUNWIND_IS_BAREMETAL=1 -D_GNU_SOURCE -U__FreeBSD__ -U__FreeBSD_cc_version -fexceptions -frtti -nostdlibinc -isystem $W/inc-cxx -isystem $W/shim"
cmake -G Ninja -S llvm18/runtimes -B cxxbuild -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER_TARGET=x86_64-scei-ps4 -DCMAKE_CXX_COMPILER_TARGET=x86_64-scei-ps4 \
 -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DCMAKE_AR=/usr/bin/llvm-ar -DCMAKE_RANLIB=/usr/bin/llvm-ranlib \
 -DCMAKE_C_FLAGS="$F" -DCMAKE_CXX_FLAGS="$F" -DLLVM_ENABLE_RUNTIMES="libunwind;libcxxabi;libcxx" -DLLVM_INCLUDE_TESTS=OFF \
 -DLIBCXX_ENABLE_SHARED=OFF -DLIBCXXABI_ENABLE_SHARED=OFF -DLIBUNWIND_ENABLE_SHARED=OFF \
 -DLIBCXX_HAS_MUSL_LIBC=ON -DLIBCXX_HAS_PTHREAD_API=ON -DLIBCXX_ENABLE_EXCEPTIONS=ON -DLIBCXXABI_ENABLE_EXCEPTIONS=ON \
 -DLIBCXX_ENABLE_FILESYSTEM=ON -DLIBCXX_ENABLE_RTTI=ON -DLIBCXXABI_ENABLE_RTTI=ON \
 -DLIBCXX_USE_COMPILER_RT=OFF -DLIBCXXABI_USE_COMPILER_RT=OFF -DLIBUNWIND_USE_COMPILER_RT=OFF \
 -DLIBCXXABI_USE_LLVM_UNWINDER=ON -DLIBCXX_CXX_ABI=libcxxabi -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON -DLIBCXXABI_ENABLE_STATIC_UNWINDER=ON \
 -DLIBCXX_INCLUDE_BENCHMARKS=OFF -DLIBCXX_INCLUDE_TESTS=OFF -DLIBCXXABI_INCLUDE_TESTS=OFF -DLIBUNWIND_INCLUDE_TESTS=OFF \
 -DLIBCXX_ENABLE_MONOTONIC_CLOCK=ON -DLIBCXX_ENABLE_THREADS=ON -DLIBCXXABI_ENABLE_THREADS=ON -DLIBUNWIND_ENABLE_THREADS=ON \
 -DCMAKE_INSTALL_PREFIX="$W/cxxinst"
ninja -C cxxbuild && ninja -C cxxbuild install

# 4) compiler-rt builtins
cmake -G Ninja -S llvm18/compiler-rt/lib/builtins -B rtbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang \
 -DCMAKE_C_COMPILER_TARGET=x86_64-scei-ps4 -DCMAKE_ASM_COMPILER_TARGET=x86_64-scei-ps4 -DCMAKE_SYSTEM_NAME=Generic \
 -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DCMAKE_AR=/usr/bin/llvm-ar -DCMAKE_RANLIB=/usr/bin/llvm-ranlib \
 -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON -DCOMPILER_RT_BAREMETAL_BUILD=ON \
 -DCMAKE_C_FLAGS="-fPIC -U__FreeBSD__ -nostdlibinc -isystem $W/inc-cxx" -DCMAKE_ASM_FLAGS="-fPIC"
ninja -C rtbuild
clang -fPIC -c tc/src/crt/crtlib.c -o crtlib.o
echo "Toolchain pronto em $W"
