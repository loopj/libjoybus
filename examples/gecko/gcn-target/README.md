# GeckoSDK GameCube Target Example

Act as a GameCube controller on EFR32 devices using the Gecko SDK.

```bash
# Replace brd2710a with your board or chip
TARGET=brd2710a

# Generate the project files
slc generate gcn_target.slcp  --with $TARGET --export-destination target/$TARGET --output-type cmake --sdk-extensions=../../..

# Build the project
cd target/$TARGET/gcn_target_cmake
cmake --workflow --preset project
```