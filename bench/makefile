.PHONY: all build micro live compare clean

# NOTE: the micro benchmarks are built the way the daemon is installed (-O3,
# NDEBUG) on top of the test suite's fakes.
FRAMEWORKS = -F/System/Library/PrivateFrameworks -framework Carbon -framework Cocoa -framework CoreServices -framework CoreVideo -framework SkyLight
GIT        = $(shell git rev-parse --short HEAD 2>/dev/null)$(shell git diff --quiet HEAD -- ../src 2>/dev/null || echo +dirty)
OUT        = ./results
STAMP      = $(shell date +%Y%m%d-%H%M%S)

all: micro

build:
	mkdir -p ./bin
	clang ./src/bench.m -o ./bin/bench -O3 -DNDEBUG -DTESTS -DPROFILE=1 -DANGRY_BENCH -DBENCH_GIT='"$(GIT)"' $(FRAMEWORKS)
	# NOTE: the window helper is an app bundle, so that it is a regular app from
	# the moment it launches and yabai observes it like any other application.
	mkdir -p ./bin/bench-windows.app/Contents/MacOS
	cp ./src/Info.plist ./bin/bench-windows.app/Contents/Info.plist
	clang ./src/windows.m -o ./bin/bench-windows.app/Contents/MacOS/bench-windows -O2 -framework Cocoa

micro: build
	mkdir -p $(OUT)
	./bin/bench --json $(OUT)/micro-$(STAMP).json $(ARGS)

live: build
	mkdir -p $(OUT)
	python3 ./live.py --json $(OUT)/live-$(STAMP).json $(ARGS)

compare:
	python3 ./compare.py $(BASE) $(NEW)

clean:
	rm -rf ./bin
