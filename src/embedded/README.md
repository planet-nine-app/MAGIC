# MAGIC's embedded (hardware) solution

An example of MAGIC protocol transmitted over Bluetooth using COTS hardware.

A Docker image is available to build the software project binary.

The project utilizes [Zephyr OS](https://docs.zephyrproject.org/latest/index.html). Zephyr OS is an open source kernel designed for embedded systems. The kernel supports numerous hardware platforms. Changing the `-b` option when executing  `west build` will compile for different [hardware platforms supported by Zephyr](https://docs.zephyrproject.org/latest/boards/index.html).

This example is utilizing the [ESP32S3 DevKitM](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/hw-reference/esp32s3/user-guide-devkitm-1.html).

## Create Docker container
```
docker build -t esp-zephyr .
```
> **&#9432; Note** The container build will take a significant amount of time to complete

## Build software in container
For a pristine build (clean build but longer build time) run the following command. 
```
docker run --rm --mount type=bind,source=$pwd/project,target=/workdir/project -w /workdir/project esp-zephyr west build -p always -b esp32s3_devkitm/esp32s3/procpu
```

## Note: $pwd is a Linux command that prints the current directory path from root. If you're on MacOS or Windows, substitute that path in instead.

For faster builds remove the pristine option `-p always`.
```
docker run --rm --mount type=bind,source=$pwd/project,target=/workdir/project -w /workdir/project esp-zephyr west build -b esp32s3_devkitm/esp32s3/procpu
```

## Flash device with built software
Plug in dongle and run west flash from container. 
```
docker run --rm --mount type=bind,source=$pwd/project,target=/workdir/project -w /workdir/project esp-zephyr west flash
```

## Run interactive Docker container
To run the container in interactive mode. 
```
docker run -it --rm --mount type=bind,source=$pwd/project,target=/workdir/project -w /workdir/project esp-zephyr
```
