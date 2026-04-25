all: build/piconet.uf2

build/piconet.uf2:
	cmake -G Ninja -B build .
	cmake --build build

flash:
	sudo picotool load build/piconet.uf2 -f -x

connect:
	( stty -echo -icanon; ncat -C 192.168.4.173 2300; stty echo icanon )

clean:
	rm -rf build
