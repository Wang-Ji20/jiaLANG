./build/main:
	make --directory src
	mkdir -p build
	mv ./src/main ./build
	make --directory src clean

run: ./build/main
	make --directory examples

clean:
	make --directory examples clean
	rm build/main
	
install:
	sudo bash ./tools/env.sh