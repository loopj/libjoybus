# GeckoSDK GameCube Host Example

Act as a GameCube Joybus host on EFR32 devices using the Gecko SDK.

```bash
# Replace brd2710a with your board or chip
TARGET=brd2710a

# Generate the project files
slc generate gcn_host.slcp  --with $TARGET --export-destination target/$TARGET --output-type cmake --sdk-extensions=../../..

# Build the project
cd target/$TARGET/gcn_host_cmake
cmake -Bbuild -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake
cmake --build build
```