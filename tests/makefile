.PHONY: clean build run all coverage

all: clean build run

clean:
	rm -rf ./bin

build:
	mkdir -p ./bin
	clang ./src/tests.m -o ./bin/tests -DTESTS -DPROFILE=1 -F/System/Library/PrivateFrameworks -framework Carbon -framework Cocoa -framework CoreServices -framework CoreVideo -framework SkyLight

run:
	./bin/tests

coverage: clean
	mkdir -p ./bin
	clang ./src/tests.m -o ./bin/tests_cov -DTESTS -DPROFILE=1 -fprofile-instr-generate -fcoverage-mapping -F/System/Library/PrivateFrameworks -framework Carbon -framework Cocoa -framework CoreServices -framework CoreVideo -framework SkyLight
	LLVM_PROFILE_FILE="./bin/tests.profraw" ./bin/tests_cov
	xcrun llvm-profdata merge -sparse ./bin/tests.profraw -o ./bin/tests.profdata
	xcrun llvm-cov report ./bin/tests_cov -instr-profile=./bin/tests.profdata
