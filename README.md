## OpenDLV microservice to simulate a camera

[![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)


## Usage

To run this microservice using Docker 
start it as follows:

```
docker run --rm -ti --init --ipc=host --net=host -v ${PWD}/myMap:/opt/map -v /tmp:/tmp -e DISPLAY=$DISPLAY chalmersrevere/opendlv-sim-camera:1_mesa --cid=111 --frame-id=0 --map-path=/opt/map --x=0.0 --z=0.095 --width=1280 --height=720 --fovy=48.8 --freq=7 --verbose
```

To run a complete camera simulation using docker-compose:
```
version: "3.6"

services:
  sim-global:
    image: chalmersrevere/opendlv-sim-global-amd64:v0.0.6
    network_mode: "host"
    command: "/usr/bin/opendlv-sim-global --cid=111 --freq=20 --frame-id=0 --x=0.0 --yaw=0.2"

  sim-motor-kiwi:
    image: chalmersrevere/opendlv-sim-motor-kiwi-amd64:v0.0.6
    network_mode: "host"
    command: "/usr/bin/opendlv-sim-motor-kiwi --cid=111 --freq=50 --frame-id=0"

  sim-camera:
    container_name: sim-camera
    image: chalmersrevere/opendlv-sim-camera:1_mesa
    ipc: "host"
    network_mode: "host"
    volumes:
      - ${PWD}/resource/example_map:/opt/map
      - /tmp:/tmp
    environment:
      - DISPLAY=${DISPLAY}
    command: "--cid=111 --frame-id=0 --map-path=/opt/map --x=0.0 --z=0.095 --width=1280 --height=720 --fovy=48.8 --freq=7 --verbose"

  kiwi-view:
    container_name: kiwi-view
    image: chalmersrevere/opendlv-vehicle-view-multi:v0.0.60
    restart: always
    network_mode: "host"
    volumes:
      - /home/pi/recordings:/opt/vehicle-view/recordings
      - /var/run/docker.sock:/var/run/docker.sock
    environment:
      - OD4SESSION_CID=111
      - OPENDLV_VEHICLE_VIEW_PLATFORM=Kiwi 
    ports:
      - "8081:8081"
```

Example to run two Kiwi cars in the same simulation (OD4 CIDs 111 and 112). 
Each car has its own web interface for control (ports 8081 and 8082), and one car 
has a simulated camera. Note the 'timemod' parameter, which makes the simulation run
slower. This might be important for example when running on VirtualBox as it uses
software rendering.
```
version: '2'

services:
  sim-global-1:
    image: chalmersrevere/opendlv-sim-global-amd64:v0.0.6
    network_mode: "host"
    command: "/usr/bin/opendlv-sim-global --cid=111 --freq=50 --timemod=0.2 --frame-id=0 --x=0.9 --y=1.6 --yaw=-3.14 --extra-cid-out=112:1"

  sim-motor-kiwi-1:
    image: chalmersrevere/opendlv-sim-motor-kiwi-amd64:v0.0.7
    network_mode: "host"
    command: "/usr/bin/opendlv-sim-motor-kiwi --cid=111 --freq=200 --timemod=0.2 --frame-id=0"

  sim-camera-1:
    container_name: sim-camera
    image: chalmersrevere/opendlv-sim-camera:1_mesa
    ipc: "host"
    network_mode: "host"
    volumes:
      - ${PWD}/resource/example_map:/opt/map
      - /tmp:/tmp
    environment:
      - DISPLAY=${DISPLAY}
    command: "--cid=111 --frame-id=0 --map-path=/opt/map --x=0.0 --z=0.095 --width=1280 --height=720 --fovy=48.8 --freq=7 --timemod=0.2 --verbose"

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
    image: chalmersrevere/opendlv-sim-global-amd64:v0.0.6
    network_mode: "host"
    command: "/usr/bin/opendlv-sim-global --cid=112 --freq=50 --timemod=0.2 --frame-id=0 --x=1.9 --y=1.6 --yaw=-3.14 --extra-cid-out=111:1"

  sim-motor-kiwi-2:
    image: chalmersrevere/opendlv-sim-motor-kiwi-amd64:v0.0.7
    network_mode: "host"
    command: "/usr/bin/opendlv-sim-motor-kiwi --cid=112 --freq=200 --timemod=0.2 --frame-id=0"

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
