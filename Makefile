d-build:
	docker build -f ./Dockerfile.traffic-light -t kos-traffic-light:1.1.1.40u20.04 .

d-build-server:
	docker build -f ./Dockerfile.traffic-server -t kos-traffic-server:1.1.1.40u20.04 .

d-run-server:
	docker run --net=host --name kos-traffic-server --user user -it --rm kos-traffic-server:1.1.1.40u20.04

develop:
	docker run --net=host --volume="`pwd`:/data" --name kos-traffic-lights -w /data --user user -it --rm kos-traffic-light:1.1.1.40u20.04 bash

get-json-parser:
	git clone https://github.com/json-parser/json-parser

prepare: get-json-parser

# run inside kos container
build-sim:
	./cross-build.sh

# run inside kos container
sim: build-sim

# run inside kos container
clean:
	rm -rf build

# run inside kos container
rebuild: clean build-sim
