## OpenDLV microservice to simulate a camera

[![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)


## Usage

To run this microservice using Docker 
start it as follows:

```
docker run --rm -ti --init --ipc=host --net=host -v ${PWD}/myMap:/opt/map -v /tmp:/tmp -e DISPLAY=$DISPLAY chalmersrevere/opendlv-sim-camera opendlv-sim-camera --cid=111 --frame-id=0 --map-path=/opt/map --x=0.072 --z=0.064 --width=1280 --height=720 --fovy=48.8 --freq=5 --verbose
```

## License

* This project is released under the terms of the GNU GPLv3 License
