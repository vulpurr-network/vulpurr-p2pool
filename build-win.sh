set -e
T=/p2pool/cmake/mingw_gcc.cmake
W="-D_WIN32_WINNT=0x0601 -DWINVER=0x0601"
if [ ! -f /p2pool/external/src/libuv/build/libuv.a ]; then
cd /p2pool/external/src/libuv && rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$T -DBUILD_TESTING=OFF -DLIBUV_BUILD_SHARED=OFF >/dev/null && make -j14 >/dev/null
fi
if [ ! -f /p2pool/external/src/libzmq/build/lib/libzmq.a ]; then
cd /p2pool/external/src/libzmq && rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$T -DCMAKE_C_FLAGS="$W" -DCMAKE_CXX_FLAGS="$W" -DWITH_LIBSODIUM=OFF -DWITH_LIBBSD=OFF -DBUILD_TESTS=OFF -DWITH_DOCS=OFF -DENABLE_DRAFTS=OFF -DBUILD_SHARED=OFF -DPOLLER=epoll -DCMAKE_POLICY_VERSION_MINIMUM=3.5 >/dev/null && make -j14 >/dev/null
fi
if [ ! -f /p2pool/external/src/curl/lib/libcurl.a ]; then
cd /p2pool/external/src/curl
cmake . -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_TOOLCHAIN_FILE=$T -DCMAKE_C_FLAGS="$W" -DCURL_DROP_UNUSED=ON -DCURL_DISABLE_TYPECHECK=ON -DPICKY_COMPILER=OFF -DBUILD_CURL_EXE=OFF -DBUILD_SHARED_LIBS=OFF -DCURL_DISABLE_INSTALL=ON -DBUILD_LIBCURL_DOCS=OFF -DBUILD_MISC_DOCS=OFF -DENABLE_CURL_MANUAL=OFF -DCURL_ZLIB=OFF -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF -DHTTP_ONLY=ON -DCURL_ENABLE_SSL=OFF -DCURL_USE_SCHANNEL=OFF -DUSE_LIBIDN2=OFF -DUSE_WIN32_IDN=OFF -DCURL_USE_LIBPSL=OFF -DCURL_USE_LIBSSH2=OFF -DBUILD_TESTING=OFF -DUSE_NGHTTP2=OFF -DBUILD_EXAMPLES=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 >/dev/null
make -j14 >/dev/null
fi
cd /p2pool && rm -rf build && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$T -DCMAKE_C_FLAGS="-Wno-error" -DCMAKE_CXX_FLAGS="-Wno-error" -DSTATIC_BINARY=ON -DWITH_GRPC=OFF -DWITH_TLS=OFF -DWITH_UPNP=OFF -DOPENSSL_NO_ASM=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DARCH_ID=x86_64
make -j14 p2pool
