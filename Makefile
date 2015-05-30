configure:
	python3.4 waf configure

build2: configure
	python3.4 waf build

install: build2
	python3.4 waf install
