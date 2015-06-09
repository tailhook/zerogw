
install: build2
	python3.4 waf install

configure:
	python3.4 waf configure

build2: configure
	python3.4 waf build

clean: configure
	python3.4 waf clean

./build/zerogw : build2
	echo build

test2 : clean ./build/zerogw
	./build/zerogw  --log-level 7 -c examples/zerogw.yaml & #-e error.log
	wget localhost:8000/chat

test3 : ./build/zerogw
	./build/zerogw  --log-level 7 -c examples/zerogw.yaml & #-e error.log
	wget localhost:8000/chat

testcon : ./build/zerogw
	./build/zerogw  --log-level 7 -PP --debug-config -c examples/zerogw.yaml -e error.log


test3:
	gcc -g3  -I./build/src/ ../gnulib/tests/test-accept4.c  src/accept4.c
