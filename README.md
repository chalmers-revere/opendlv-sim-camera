## OpenDLV microservice to simulate a camera

[![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)


## Usage

To run this microservice using Docker 
start it as follows:

```
docker run --rm -ti --init --ipc=host --net=host -v ${PWD}/myMap:/opt/map -v /tmp:/tmp -e DISPLAY=$DISPLAY chalmersrevere/opendlv-sim-camera:1_mesa --cid=111 --frame-id=0 --map-path=/opt/map --x=0.072 --z=0.064 --width=1280 --height=720 --fovy=48.8 --freq=20 --verbose
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
        command: "--cid=111 --frame-id=0 --map-path=/opt/map --x=0.072 --z=0.064 --width=1280 --height=720 --fovy=48.8 --freq=5 --verbose"

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


## License

* This project is released under the terms of the GNU GPLv3 License
