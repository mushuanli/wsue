# 1. Fix compile error
```
$ xcrun --sdk iphoneos --show-sdk-path 
  xcrun: error: SDK "iphoneos" cannot be located 
  xcrun: error: SDK "iphoneos" cannot be located 
  xcrun: error: unable to lookup item 'Path' in SDK 'iphoneos' 
  
$  xcode-select --print-path 
  /Library/Developer/CommandLineTools 
  
$ xcodebuild -showsdks  
  xcode-select: error: tool 'xcodebuild' requires Xcode, but active developer directory '/Library/Developer/CommandLineTools' is a command line tools instance
  
$ sudo -i
Password:
rains-mac-mini:~ root#  xcode-select --switch /Applications/Xcode.app/Contents/Developer/
rains-mac-mini:~ root#  xcrun --sdk iphoneos --show-sdk-path
/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS13.1.sdk
```

#2. mac remote ssh
```
# start
sudo  /System/Library/CoreServices/RemoteManagement/ARDAgent.app/Contents/Resources/kickstart \
-activate -configure -access -on \
-clientopts -setvnclegacy -vnclegacy yes \
-clientopts -setvncpw -vncpw mypasswd \
-restart -agent -privs -all
, can use tigerVNC
# access
#stop
sudo /System/Library/CoreServices/RemoteManagement/ARDAgent.app/Contents/Resources/kickstart \
-deactivate -configure -access -off
```

# 3. install homebrew vim and change PS1

```
$ cat ~/.bashrc 
export PS1='\033[1;;93;44mMBP:\033[0m\033[01;34m\w\[\033[00m\]\$ '
```

```
 /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
 brew install macvim
```



