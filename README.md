# libsmb2-build

* ./build.sh arm
* ./build.sh arm64
* ./build.sh x86

## win32

* ./Configure no-shared --cross-compile-prefix=i686-w64-mingw32- mingw


```
 CMakeLists.txt | 4 ++++
 1 file changed, 4 insertions(+)

diff --git a/CMakeLists.txt b/CMakeLists.txt
index 27a1a5b..4751f94 100644
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -452,6 +452,8 @@ if (srt_libspec_shared)
 	target_link_libraries(${TARGET_srt}_shared PRIVATE ${SSL_LIBRARIES})
 	if (MICROSOFT)
 		target_link_libraries(${TARGET_srt}_shared PRIVATE ws2_32.lib)
+	elseif (MINGW)
+		target_link_libraries(${TARGET_srt}_shared PRIVATE wsock32.lib ws2_32.lib)
 	endif()
 endif()
 
@@ -476,6 +478,8 @@ if (srt_libspec_static)
 	target_link_libraries(${TARGET_srt}_static PRIVATE ${SSL_LIBRARIES})
 	if (MICROSOFT)
 		target_link_libraries(${TARGET_srt}_static PRIVATE ws2_32.lib)
+	elseif (MINGW)
+		target_link_libraries(${TARGET_srt}_static PRIVATE wsock32.lib ws2_32.lib)
 	endif()
 endif()
 
-- 
2.18.0
```

* CC=i686-w64-mingw32-gcc cmake -DCMAKE_TOOLCHAIN_FILE=../cross-mingw.txt -DOPENSSL_INCLUDE_DIR=/projects/github/libsmb2-build/openssl-mingw32/include -DCMAKE_SYSTEM_NAME=Windows ..
* https://www.npcglib.org/~stathis/blog/precompiled-openssl/
* cmake -G "Visual Studio 14 2015" -DOPENSSL_INCLUDE_DIR=D:\jiajia-source\repos\openssl-1.1.0f-vs2015\include -DBUILD_SHARED_LIBS=1 -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=TRUE ..
* cmake --build . --config RelWithDebInfo
