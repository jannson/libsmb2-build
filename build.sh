#!/bin/sh

JAVA_HOME=/projects/jdk/jdk1.8.0_60
export JAVA_HOME
export ANDROID_HOME=/projects/jdk/android/Sdk
export ANDROID_NDK=/projects/jdk/android/Sdk/ndk/21.3.6528147
export ANDROID_NDK_HOME=/projects/jdk/android/Sdk/ndk/21.3.6528147
CMAKE_HOME=/projects/jdk/android/Sdk/cmake/3.10.2.4988404/bin
CLASSPATH=.:$JAVA_HOME/lib:$JAVA_HOME/lib/dt.jar:$JAVA_HOME/lib/tools.jar
PATH=$CMAKE_HOME:$JAVA_HOME/bin:$ANDROID_HOME/platform-tools:$PATH

SMB_TARGET=$1
case $SMB_TARGET in
  arm)
    if [ ! -d openssl-arm ]; then
      tar -zxf openssl-1_1_1d.tar.gz
      mv openssl-OpenSSL_1_1_1d openssl-arm
    fi
    ;;
  arm64)
    ;;
  x86)
    ;;
  *)
    ./build.sh arm|arm64|x86 supported only
    ;;
esac
