# iOS Metal Cube Example

This example builds a native iOS app that renders the shared cube scene through the Metal backend.
It consumes an external upstream `raylib` checkout and generates an `rcore.c` overlay in the build
directory; it does not edit files in `raylib/src`.

```sh
cmake -S raylib-backends/examples/ios \
  -B /tmp/raylib-backends-ios-metal \
  -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DRAYLIB_DIR="$PWD/raylib"

cmake --build /tmp/raylib-backends-ios-metal --config Debug
```

For a physical device, use `-DCMAKE_OSX_SYSROOT=iphoneos` and provide normal Xcode signing settings.
