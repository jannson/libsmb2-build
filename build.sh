#!/bin/sh

JAVA_HOME=/projects/jdk/jdk1.8.0_60
export JAVA_HOME
export ANDROID_HOME=/projects/jdk/android/Sdk
export ANDROID_NDK=/projects/jdk/android/Sdk/ndk/21.3.6528147
export ANDROID_NDK_HOME=/projects/jdk/android/Sdk/ndk/21.3.6528147
CMAKE_HOME=/projects/jdk/android/Sdk/cmake/3.10.2.4988404/bin
CLASSPATH=.:$JAVA_HOME/lib:$JAVA_HOME/lib/dt.jar:$JAVA_HOME/lib/tools.jar
PATH=$CMAKE_HOME:$JAVA_HOME/bin:$ANDROID_HOME/platform-tools:$PATH

CURR=`pwd`
CC=clang
toolchains_path=$(python toolchains_path.py --ndk ${ANDROID_NDK_HOME})
PATH=$toolchains_path/bin:$PATH
ANDROID_API=21

SMB_TARGET=$1
case $SMB_TARGET in
  arm)
    architecture=android-arm
    TARGET_OPENSSL=$CURR/openssl-arm
    if [ ! -d openssl-arm ]; then
      tar -zxf openssl-1_1_1d.tar.gz
      mv openssl-OpenSSL_1_1_1d openssl-arm
    fi
    SMB2_BUILD=$CURR/libsmb2/build-arm
    SMB2_API=armeabi-v7a
    ;;
  arm64)
    architecture=android-arm64
    TARGET_OPENSSL=$CURR/openssl-arm64
    if [ ! -d openssl-arm64 ]; then
      tar -zxf openssl-1_1_1d.tar.gz
      mv openssl-OpenSSL_1_1_1d openssl-arm64
    fi
    SMB2_BUILD=$CURR/libsmb2/build-arm64
    SMB2_API=arm64-v8a
    ;;
  x86)
    architecture=android-x86
    TARGET_OPENSSL=$CURR/openssl-x86
    if [ ! -d openssl-x86 ]; then
      tar -zxf openssl-1_1_1d.tar.gz
      mv openssl-OpenSSL_1_1_1d openssl-x86
    fi
    SMB2_BUILD=$CURR/libsmb2/build-x86
    SMB2_API=x86
    ;;
  *)
    ./build.sh arm|arm64|x86 supported only
    ;;
esac

mkdir -p $SMB2_BUILD
cd $TARGET_OPENSSL && ./Configure ${architecture} -D__ANDROID_API__=$ANDROID_API && make && cd $CURR
cd $SMB2_BUILD && cmake .. -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake -DANDROID_STL=c++_shared -DANDROID_TOOLCHAIN=clang -DANDROID_PLATFORM=android-21 -DANDROID_ABI=$SMB2_API -DOPENSSL_INCLUDE_DIR=$TARGET_OPENSSL/include && make && cd $CURR

