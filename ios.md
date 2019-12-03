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
