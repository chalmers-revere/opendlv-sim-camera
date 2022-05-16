# OpenDLV microservice to simulate a camera

[![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)


## Usage

To run this microservice using Docker start it as shown below, depending on what GPU is present on the machine or if using software rendering. Make sure to first run `xhost +` to allow Docker to display the graphical window. The examples were tested on Wayland.

Note: The output image will be painted gray as long as there is no `opendlv-sim-global` microservice is running on the given `cid`.

### Intel GPUs

Run the camera simulation with the following

```
docker run --rm -ti --init --device /dev/dri --ipc=host --net=host -v ${PWD}/myMap:/opt/map -v /tmp:/tmp -e DISPLAY=$DISPLAY chalmersrevere/opendlv-sim-camera:v0.0.2-mesa --cid=111 --frame-id=0 --map-path=/opt/map --x=0.0 --z=0.095 --width=1280 --height=720 --fovy=48.8 --freq=7.5 --verbose
```
Note: You need to add `--device /dev/dri` to allow Docker to use the Intel GPU.

### Nvidia GPUs

Run the camera simulation with the following

```
docker run --rm -ti --init --gpus all --ipc=host --net=host -v ${PWD}/myMap:/opt/map -v /tmp:/tmp -e DISPLAY=$DISPLAY chalmersrevere/opendlv-sim-camera:v0.0.2-nvidia --cid=111 --frame-id=0 --map-path=/opt/map --x=0.0 --z=0.095 --width=1280 --height=720 --fovy=48.8 --freq=7.5 --verbose
```
Note: You need to add `--gpus all` to allow Docker to use the Nvidia GPU.

#### Troubleshooting

Make sure that Nvidia works in Docker by running
```
docker run --gpus all nvidia/cuda:11.3.0-runtime-ubuntu20.04 nvidia-smi
```
There should be terminal output with information about the Nvidia GPU installed on the system. If not, please check instructions on how to integrate Nvidia support for your version of Docker.

### Software rendering

Useful when for example running in VirtualBox, as virtual machines are not able to access the GPUs of the host system. Run the camera simulation with the following
```
docker run --rm -ti --init --ipc=host --net=host -v ${PWD}/myMap:/opt/map -v /tmp:/tmp -e DISPLAY=$DISPLAY chalmersrevere/opendlv-sim-camera:v0.0.2-swr --cid=111 --frame-id=0 --map-path=/opt/map --x=0.0 --z=0.095 --width=1280 --height=720 --fovy=48.8 --freq=7.5 --verbose
```

## Using Docker compose

To run a complete camera simulation using `docker-compose`:
```
version: "3.6"

services:
  sim-global:
    image: chalmersrevere/opendlv-sim-global-amd64:v0.0.7
    network_mode: "host"
    command: "/usr/bin/opendlv-sim-global --cid=111 --freq=20 --frame-id=0 --x=0.0 --yaw=0.2"

  sim-motor-kiwi:
    image: chalmersrevere/opendlv-sim-motor-kiwi-amd64:v0.0.7
    network_mode: "host"
    command: "/usr/bin/opendlv-sim-motor-kiwi --cid=111 --freq=50 --frame-id=0"

  sim-camera:
    container_name: sim-camera
    image: chalmersrevere/opendlv-sim-camera:v0.0.2-mesa
    ipc: "host"
    network_mode: "host"
    volumes:
      - ${PWD}/resource/example_map:/opt/map
      - /tmp:/tmp
    devices:
      - /dev/dri
    environment:
      - DISPLAY=${DISPLAY}
    command: "--cid=111 --frame-id=0 --map-path=/opt/map --x=0.0 --z=0.095 --width=1280 --height=720 --fovy=48.8 --freq=7.5 --verbose"

  opendlv-kiwi-view:
    image: chrberger/opendlv-kiwi-view-webrtc-multi:v0.0.6
    network_mode: "host"
    volumes:
      - ~/recordings:/opt/vehicle-view/recordings
      - /var/run/docker.sock:/var/run/docker.sock
    environment:
      - PORT=8081
      - OD4SESSION_CID=111
      - PLAYBACK_OD4SESSION_CID=253
```

Example to run two Kiwi cars in the same simulation (OD4 CIDs 111 and 112). 
Each car has its own web interface for control (ports 8081 and 8082), and one car 
has a simulated camera. Note the `timemod` parameter, which makes the simulation run
slower. This might be important for example when running on a virtual machine as it uses
software rendering.
```
version: '2'

services:
  sim-global-1:
    image: chalmersrevere/opendlv-sim-global-amd64:v0.0.7
    network_mode: "host"
    command: "/usr/bin/opendlv-sim-global --cid=111 --freq=50 --timemod=1.0 --frame-id=0 --x=0.9 --y=1.6 --yaw=-3.14 --extra-cid-out=112:1"

  sim-motor-kiwi-1:
    image: chalmersrevere/opendlv-sim-motor-kiwi-amd64:v0.0.7
    network_mode: "host"
    command: "/usr/bin/opendlv-sim-motor-kiwi --cid=111 --freq=200 --timemod=1.0 --frame-id=0"

  sim-camera-1:
    container_name: sim-camera
    image: chalmersrevere/opendlv-sim-camera:v0.0.2-mesa
    ipc: "host"
    network_mode: "host"
    volumes:
      - ${PWD}/resource/example_map:/opt/map
      - /tmp:/tmp
    devices:
      - /dev/dri
    environment:
      - DISPLAY=${DISPLAY}
    command: "--cid=111 --frame-id=0 --map-path=/opt/map --x=0.0 --z=0.095 --width=1280 --height=720 --fovy=48.8 --freq=7.5 --timemod=1.0 --verbose"

  opendlv-kiwi-view-1:
    image: chrberger/opendlv-kiwi-view-webrtc-multi:v0.0.6
    network_mode: "host"
    volumes:
      - ~/recordings:/opt/vehicle-view/recordings
      - /var/run/docker.sock:/var/run/docker.sock
    environment:
      - PORT=8081
      - OD4SESSION_CID=111
      - PLAYBACK_OD4SESSION_CID=253

  
  sim-global-2:
    image: chalmersrevere/opendlv-sim-global-amd64:v0.0.7
    network_mode: "host"
    command: "/usr/bin/opendlv-sim-global --cid=112 --freq=50 --timemod=1.0 --frame-id=0 --x=1.9 --y=1.6 --yaw=-3.14 --extra-cid-out=111:1"

  sim-motor-kiwi-2:
    image: chalmersrevere/opendlv-sim-motor-kiwi-amd64:v0.0.7
    network_mode: "host"
    command: "/usr/bin/opendlv-sim-motor-kiwi --cid=112 --freq=200 --timemod=1.0 --frame-id=0"

  opendlv-kiwi-view-2:
    image: chrberger/opendlv-kiwi-view-webrtc-multi:v0.0.6
    network_mode: "host"
    volumes:
      - ~/recordings:/opt/vehicle-view/recordings
      - /var/run/docker.sock:/var/run/docker.sock
    environment:
      - PORT=8082
      - OD4SESSION_CID=112
      - PLAYBACK_OD4SESSION_CID=254
```


## License

* This project is released under the terms of the GNU GPLv3 License
