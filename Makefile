##################################################################################################
#
#  ximodem — convenience wrapper around the CMake build
#
#  make               release build of libximodem + the CLI tools  (default)
#  make debug         debug build (-O1 -g, asserts)
#  make test          release build, then run the asserting self-tests
#  make test-conformance   run upstream Zimodem's test.py against libximodem
#                          (needs pyserial + a resolvable hostname + ./tools/ not in git repo)
#  make clean         remove compiled objects, keep the CMake cache
#  make distclean     remove the whole build tree and generated sources
#  make help          show this list
#
#  Options (append on the command line):
#    JOBS=N            parallel compile jobs            (default: all cores)
#    NO_SSH=1          build without the SSH client
#    NO_TLS=1          build WiFiClientSecure as a plaintext stub
#
##################################################################################################

BUILD_DIR   := build
JOBS        ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

CMAKE_FLAGS :=
ifeq ($(NO_SSH),1)
CMAKE_FLAGS += -DXIMODEM_NO_SSH=ON
endif
ifeq ($(NO_TLS),1)
CMAKE_FLAGS += -DXIMODEM_NO_TLS=ON
endif

.PHONY: all release debug test test-conformance clean distclean help

FLOWTEST_BYTES ?= 60000

all: release

release:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release $(CMAKE_FLAGS)
	cmake --build $(BUILD_DIR) -j $(JOBS)
	@cmake -E echo "libximodem.so and ximodem-* tools are in ./$(BUILD_DIR)"

debug:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug $(CMAKE_FLAGS)
	cmake --build $(BUILD_DIR) -j $(JOBS)

test: release
	@if [ ! -f tools/smoke.c ]; then \
	   echo "make test: tools/ is not present (local-only, gitignored) -- nothing to run"; \
	   exit 0; \
	 fi
	@set -e; cd $(BUILD_DIR); \
	 ./ximodem-smoke; \
	 ./ximodem-baudtest; \
	 ./ximodem-nettest; \
	 ./ximodem-bannertest; \
	 if [ -x ./ximodem-dialtest ]; then ./ximodem-dialtest; fi; \
	 if [ -x ./ximodem-pbdialtest ]; then ./ximodem-pbdialtest; fi; \
	 if [ -x ./ximodem-sigtest ];  then ./ximodem-sigtest;  fi; \
	 if [ -x ./ximodem-flowtest ]; then ./ximodem-flowtest $(FLOWTEST_BYTES); fi; \
	 if [ -x ./ximodem-ratetest ]; then ./ximodem-ratetest; fi
	@echo "all self-tests passed"

test-conformance: release
	@test -f tools/conformance.py || { \
	   echo "make test-conformance: tools/ is not present (local-only, gitignored)"; exit 1; }
	python3 tools/conformance.py

clean:
	@if [ -d $(BUILD_DIR) ]; then cmake --build $(BUILD_DIR) --target clean; \
	else cmake -E echo "nothing to clean"; fi

distclean:
	cmake -E rm -rf $(BUILD_DIR) src/generated

help:
	@echo 'ximodem build targets:'
	@echo '  make            release build of libximodem + the CLI tools  (default)'
	@echo '  make debug      debug build (-O1 -g, asserts)'
	@echo '  make test       release build, then run the smoke test'
	@echo '  make clean      remove compiled objects, keep the CMake cache'
	@echo '  make distclean  remove the whole build tree and generated sources'
	@echo ''
	@echo 'options:  JOBS=N   NO_SSH=1   NO_TLS=1'
