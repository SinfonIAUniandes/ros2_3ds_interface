# Building From Source

## Requirements

- devkitPro with devkitARM and libctru
- CMake 3.16 or newer
- GNU Make
- Git
- A Linux environment or equivalent devkitPro-supported shell

The application requires the 3DS port from
[SinfonIAUniandes/cyclone3dds](https://github.com/SinfonIAUniandes/cyclone3dds).
Keep the Cyclone DDS and application repositories as sibling directories unless
you plan to override the Makefile paths.

## Clone

```sh
git clone https://github.com/SinfonIAUniandes/cyclone3dds.git cyclonedds_3ds
git clone https://github.com/SinfonIAUniandes/ros2_3ds_interface.git
```

## Build Cyclone DDS

```sh
cd cyclonedds_3ds
cmake -S . -B build-3ds \
  -DCMAKE_TOOLCHAIN_FILE=ports/3ds/toolchain.cmake \
  -DWITH_NINTENDO_3DS=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_IDLC=OFF \
  -DBUILD_DDSPERF=OFF \
  -DBUILD_TESTING=OFF \
  -DENABLE_SECURITY=OFF \
  -DENABLE_IPV6=OFF \
  -DENABLE_SOURCE_SPECIFIC_MULTICAST=OFF
cmake --build build-3ds --target ddsc -j2
```

This produces `build-3ds/lib/libddsc.a`.

## Build the Application

```sh
cd ../ros2_3ds_interface
export DEVKITPRO=/opt/devkitpro
export DEVKITARM="$DEVKITPRO/devkitARM"
make clean
make -j2
```

The main output is `ros2_3ds_interface.3dsx`.

If the Cyclone repository or build directory has a different location:

```sh
make -j2 \
  CYCLONEDDS_SOURCE=/path/to/cyclonedds_3ds \
  CYCLONEDDS_BUILD=/path/to/cyclonedds_3ds/build-3ds
```

## Generated DDS Types

The C files under `generated/` are host-generated Cyclone DDS descriptors and
are compiled directly into the target. The 3DS build does not run `idlc` or ROS
IDL generators.

When regenerating types, use a host `idlc` compatible with the Cyclone DDS fork
and review TypeInformation and TypeMapping changes before replacing committed
files.