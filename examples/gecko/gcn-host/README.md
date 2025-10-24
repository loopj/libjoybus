# GeckoSDK Host Example

Act as a Joybus host on EFR32 devices using the Gecko SDK.

```bash
# Generate the project files
# Replace EFR32MG22C224F512IM40 with your device
slc generate --with EFR32MG22C224F512IM40 --project-file libjoybus_example.slcp --export-destination slc_project

# Build the project
cmake -Bbuild -DCMAKE_TOOLCHAIN_FILE=slc_project/libjoybus_example_cmake/toolchain.cmake
cmake --build build --target app
```