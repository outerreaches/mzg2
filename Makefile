# MZG2 build: portable core + adapters + CPU tests, and an optional HIP
# decoder. The CPU targets need no GPU, no ROCm and no model weights.

CC ?= cc
AR ?= ar
HIPCC ?= hipcc
GPU_ARCH ?= gfx1151
CLANG ?= clang
BUILD_DIR ?= .
OUTPUT_PREFIX := $(if $(filter .,$(BUILD_DIR)),,$(BUILD_DIR)/)

INCLUDES := -Icore/include -Iadapters/kimi-k3/include
CFLAGS ?= -O2 -g
C_WARNINGS := -std=c11 -Wall -Wextra -Wpedantic -Werror
HIPFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Werror
HIPFLAGS += --offload-arch=$(GPU_ARCH) $(INCLUDES)

CORE_SOURCES := core/src/profile.c core/src/model.c core/src/block.c \
                core/src/codec.c core/src/store.c
ADAPTER_SOURCES := adapters/kimi-k3/kimi_k3.c
LIB_OBJECTS := $(addprefix $(OUTPUT_PREFIX),$(CORE_SOURCES:.c=.o) $(ADAPTER_SOURCES:.c=.o))

LIBRARY := $(OUTPUT_PREFIX)libmzg2.a
TEST_NAMES := test_format test_codec test_malformed test_store test_vectors test_api test_decoder_cases
TESTS := $(addprefix $(OUTPUT_PREFIX)tests/,$(TEST_NAMES))
TOOLS := $(addprefix $(OUTPUT_PREFIX)tools/,mzg2_vectors mzg2_store_check)

VECTOR_DIR := tests/vectors

.PHONY: all lib test tools vectors hip hip-test-build sanitize fuzz fuzz-smoke clean

all: lib tools test

lib: $(LIBRARY)

$(LIBRARY): $(LIB_OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(OUTPUT_PREFIX)%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C_WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

HEADERS := core/include/mzg2/format.h core/include/mzg2/mzg2.h \
           adapters/kimi-k3/include/mzg2/adapters/kimi_k3.h \
           tests/harness.h tests/fixture.h tests/sidecar.h tests/vectors.h \
           tests/decoder_cases.h tests/malformed_cases.h

$(LIB_OBJECTS): $(HEADERS) core/src/internal.h

$(OUTPUT_PREFIX)tests/%: tests/%.c $(LIBRARY) $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C_WARNINGS) $(INCLUDES) -Itests $< $(LIBRARY) $(LDFLAGS) $(LDLIBS) -o $@

$(OUTPUT_PREFIX)tests/test_decoder_cases: tests/test_decoder_cases.c $(OUTPUT_PREFIX)tests/decoder_cases.o $(LIBRARY) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C_WARNINGS) $(INCLUDES) -Itests $< $(OUTPUT_PREFIX)tests/decoder_cases.o $(LIBRARY) $(LDFLAGS) $(LDLIBS) -o $@

$(OUTPUT_PREFIX)tests/decoder_cases.o: $(HEADERS)

$(OUTPUT_PREFIX)tools/%: tools/%.c $(LIBRARY) $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C_WARNINGS) $(INCLUDES) -Itests $< $(LIBRARY) $(LDFLAGS) $(LDLIBS) -o $@

tools: $(TOOLS)

# Golden vectors are committed; this regenerates them. Review any diff: a
# changed vector means a changed bitstream.
vectors: $(OUTPUT_PREFIX)tools/mzg2_vectors
	mkdir -p $(VECTOR_DIR)
	./$(OUTPUT_PREFIX)tools/mzg2_vectors $(VECTOR_DIR)

test: $(TESTS)
	@status=0; \
	for binary in $(TESTS); do \
		echo "== $$binary"; \
		./$$binary || status=1; \
	done; \
	exit $$status

# The HIP decoder. Compiling needs ROCm; running needs a wave32 GPU.
hip: $(OUTPUT_PREFIX)core/src/decode_hip.o

$(OUTPUT_PREFIX)core/src/decode_hip.o: core/src/decode_hip.hip $(HEADERS) core/include/mzg2/decode_hip.h
	@mkdir -p $(dir $@)
	$(HIPCC) $(HIPFLAGS) -c $< -o $@

# Build only. Neither this target nor the default/CPU targets launch a GPU.
hip-test-build: $(OUTPUT_PREFIX)tests/test_hip

$(OUTPUT_PREFIX)tests/test_hip.o: tests/test_hip.hip $(HEADERS) core/include/mzg2/decode_hip.h
	@mkdir -p $(dir $@)
	$(HIPCC) $(HIPFLAGS) -Itests -c $< -o $@

$(OUTPUT_PREFIX)tests/test_hip: $(OUTPUT_PREFIX)tests/test_hip.o $(OUTPUT_PREFIX)core/src/decode_hip.o $(OUTPUT_PREFIX)tests/decoder_cases.o $(LIBRARY)
	$(HIPCC) $(HIPFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

sanitize:
	$(MAKE) BUILD_DIR=build/sanitize CC="$(CLANG)" \
	  CFLAGS="-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all" \
	  LDFLAGS="-fsanitize=address,undefined" test

FUZZ_NAMES := block model roundtrip store
FUZZ_BINS := $(addprefix build/fuzz/fuzz_,$(FUZZ_NAMES))
FUZZ_FLAGS := -O1 -g -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all
FUZZ_RUNS ?= 2000

build/fuzz/fuzz_%: tests/fuzz/fuzz_%.c $(CORE_SOURCES) $(ADAPTER_SOURCES) $(HEADERS) core/src/internal.h
	@mkdir -p $(dir $@)
	$(CLANG) $(C_WARNINGS) $(INCLUDES) -Itests $(FUZZ_FLAGS) $< $(CORE_SOURCES) $(ADAPTER_SOURCES) -o $@

build/fuzz/seeds: tests/fuzz/seeds.c $(CORE_SOURCES) $(ADAPTER_SOURCES) $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(C_WARNINGS) $(INCLUDES) -Itests -O1 $< $(CORE_SOURCES) $(ADAPTER_SOURCES) -o $@

fuzz: $(FUZZ_BINS) build/fuzz/seeds

fuzz-smoke: fuzz
	python3 tests/fuzz/run.py --runs $(FUZZ_RUNS)

-include $(LIB_OBJECTS:.o=.d)

clean:
	rm -f $(LIB_OBJECTS) $(LIB_OBJECTS:.o=.d) $(LIBRARY) $(TESTS) $(TOOLS) $(OUTPUT_PREFIX)core/src/decode_hip.o $(OUTPUT_PREFIX)tests/decoder_cases.o $(OUTPUT_PREFIX)tests/decoder_cases.d $(OUTPUT_PREFIX)tests/test_hip.o $(OUTPUT_PREFIX)tests/test_hip
