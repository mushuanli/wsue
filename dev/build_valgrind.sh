#!/usr/bin/env bash
#set -x
function extract() {
     if [ -f "$1" ] ; then
         case "$1" in
             *.tar.bz2)   tar xvjf "$1"     ;;
             *.tar.gz)    tar xvzf "$1"     ;;
             *.bz2)       bunzip2 "$1"      ;;
             *.rar)       unrar x "$1"      ;;
             *.gz)        gunzip "$1"       ;;
             *.tar)       tar xvf "$1"      ;;
             *.tbz2)      tar xvjf "$1"     ;;
             *.tgz)       tar xvzf "$1"     ;;
             *.zip)       unzip "$1"        ;;
             *.Z)         uncompress "$1"   ;;
             *.7z)        7z x "$1"         ;;
             *)           echo "$1 cannot be extracted via >extract<" ;;
         esac
     else
         echo "'$1' is not a valid file"
     fi
}

VALGRIND_VERSION="3.13.0"
VALGRIND_EXTENSION=".tar.bz2"
VALGRIND_DIRECTORY="valgrind-${VALGRIND_VERSION}"
VALGRIND_TARBALL="valgrind-${VALGRIND_VERSION}${VALGRIND_EXTENSION}"

# Only download Valgrind tarball again if not already downloaded
if [[ ! -f "${VALGRIND_TARBALL}" ]]; then
  #wget -v -nc "http://valgrind.org/downloads/${VALGRIND_TARBALL}"
  wget -v -nc "ftp://sourceware.org/pub/valgrind/${VALGRIND_TARBALL}"
fi

# Only extract Valgrind tarball again if not already extracted
if [[ ! -d "$VALGRIND_DIRECTORY" ]]; then
  extract "$VALGRIND_TARBALL"
fi

# Ensure ANDROID_NDK_HOME is set (the check does not work for me, remember to modify path for ANDROID_NDK_HOME)
# if [[ ! -z "$ANDROID_NDK_HOME" ]]; then
  export ANDROID_NDK_HOME="/opt/android/android-ndk-r13"
# fi

# Ensure ANDOID_SDK_HOME is set (the check does not work for me, remember to modify path for ANDROID_SDK_HOME)
# if [[ ! -z "$ANDROID_SDK_HOME" ]]; then
  export ANDROID_SDK_HOME="/opt/android/sdk/"
# fi

if [[ ! -d "$VALGRIND_DIRECTORY" ]]; then
  echo "Problem with extracting Valgrind from $VALGRIND_TARBALL into $VALGRIND_DIRECTORY!!!"
  exit -1
fi

# Move to extracted directory
cd "$VALGRIND_DIRECTORY"
# ARM Toolchain
ARCH_ABI="x86-4.9"
export AR="$ANDROID_NDK_HOME/toolchains/${ARCH_ABI}/prebuilt/linux-x86_64/bin/i686-linux-android-ar"
export LD="$ANDROID_NDK_HOME/toolchains/${ARCH_ABI}/prebuilt/linux-x86_64/bin/i686-linux-android-ld"
export CC="$ANDROID_NDK_HOME/toolchains/${ARCH_ABI}/prebuilt/linux-x86_64/bin/i686-linux-android-gcc"
export CXX="$ANDROID_NDK_HOME/toolchains/${ARCH_ABI}/prebuilt/linux-x86_64/bin/i686-linux-android-g++"

[[ ! -d "$ANDROID_NDK_HOME" || ! -f "$AR" || ! -f "$LD" || ! -f "$CC" || ! -f "$CXX" ]] && echo "Make sure AR, LD, CC, CXX variables are defined correctly. Ensure ANDROID_NDK_HOME is defined also" && exit -1

# Configure build
ANDROID_PLATFORM=android-19 # android-21 has a bug so let's do it with 19
export CPPFLAGS="--sysroot=$ANDROID_NDK_HOME/platforms/${ANDROID_PLATFORM}/arch-x86"
export CFLAGS="--sysroot=$ANDROID_NDK_HOME/platforms/${ANDROID_PLATFORM}/arch-x86 -fno-pic"

BUILD=true # don't care, let's just build it!

if [[ "$BUILD" = true ]];
then
  ./configure --prefix="/data/local/Inst" \
  --host="i686-android-linux" \
  --target="i686-android-linux" \
  --with-tmpdir="/sdcard" # bug: there was a space here

  [[ $? -ne 0 ]] && echo "Can't configure!" && exit -1

  # Determine the number of jobs (commands) to be run simultaneously by GNU Make
  NO_CPU_CORES=$(grep -c ^processor /proc/cpuinfo)

  if [ $NO_CPU_CORES -le 8 ]; then
    JOBS=$(($NO_CPU_CORES+1))
  else
    JOBS=${NO_CPU_CORES}
  fi

  # Compile Valgrind
  make -j "${JOBS}"

  [[ $? -ne 0 ]] && echo "Can't compile!" && exit -1

  # Install Valgrind locally
  make -j "${JOBS}" install DESTDIR="$(pwd)/Inst"
  [[ $? -ne 0 ]] && echo "Can't install!" && exit -1
fi

echo "success finish build valgrind"
exit 0
# Push local Valgrind installtion to the phone (if it exists, just overwrite it)
#if [[ $(adb shell ls -ld /data/local/Inst/bin/valgrind) = *"No such file or directory"* ]];
#then
  adb root
  adb remount
  adb shell "[ ! -d /data/local/Inst ] && mkdir /data/local/Inst"
  adb push Inst /
  adb shell "ls -l /data/local/Inst"

  # Ensure Valgrind on the phone is running
  adb shell "/data/local/Inst/bin/valgrind --version"

  # Add Valgrind executable to PATH (this might fail and indeed it fails..)
  adb shell "export PATH=$PATH:/data/local/Inst/bin/"
#fi

