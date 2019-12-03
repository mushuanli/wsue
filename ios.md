# 1. Fix compile error
[rain@rains-mac-mini ~/p4/60rel/iUnia/3rd_party/openssl$ xcrun --sdk iphoneos --show-sdk-path
xcrun: error: SDK "iphoneos" cannot be located
xcrun: error: SDK "iphoneos" cannot be located
xcrun: error: unable to lookup item 'Path' in SDK 'iphoneos'
[rain@rains-mac-mini ~/p4/60rel/iUnia/3rd_party/openssl$  xcode-select --print-path
/Library/Developer/CommandLineTools
[rain@rains-mac-mini ~/p4/60rel/iUnia/3rd_party/openssl$ xcodebuild -showsdks
xcode-select: error: tool 'xcodebuild' requires Xcode, but active developer directory '/Library/Developer/CommandLineTools' is a command line tools instance
[rain@rains-mac-mini ~/p4/60rel/iUnia/3rd_party/openssl$ sudo -i
Password:
rains-mac-mini:~ root#  xcode-select --switch /Applications/Xcode.app/Contents/Developer/
rains-mac-mini:~ root#  xcrun --sdk iphoneos --show-sdk-path
/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS13.1.sdk

