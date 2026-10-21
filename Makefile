# MZG2 build: portable core + adapters + CPU tests, and an optional HIP
# decoder. The CPU targets need no GPU, no ROCm and no model weights.

CC ?= cc
AR ?= ar
HIPCC ?= hipcc
GPU_ARCH ?= gfx1151

INCLUDES := -Icore/include -Iadapters/kimi-k3/include
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror $(INCLUDES)
HIPFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Werror
HIPFLAGS += --offload-arch=$(GPU_ARCH) $(INCLUDES)

CORE_SOURCES := core/src/profile.c core/src/model.c core/src/block.c \
                core/src/codec.c core/src/store.c
ADAPTER_SOURCES := adapters/kimi-k3/kimi_k3.c
LIB_OBJECTS := $(CORE_SOURCES:.c=.o) $(ADAPTER_SOURCES:.c=.o)

LIBRARY := libmzg2.a
TESTS := tests/test_format tests/test_codec tests/test_malformed \
         tests/test_store tests/test_vectors
TOOLS := tools/mzg2_vectors tools/mzg2_store_check

VECTOR_DIR := tests/vectors

.PHONY: all lib test tools vectors hip clean

all: lib tools test

lib: $(LIBRARY)

$(LIBRARY): $(LIB_OBJECTS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

HEADERS := core/include/mzg2/format.h core/include/mzg2/mzg2.h \
           adapters/kimi-k3/include/mzg2/adapters/kimi_k3.h \
           tests/harness.h tests/fixture.h tests/sidecar.h tests/vectors.h

$(LIB_OBJECTS): $(HEADERS) core/src/internal.h

tests/%: tests/%.c $(LIBRARY) $(HEADERS)
	$(CC) $(CFLAGS) -Itests $< $(LIBRARY) -o $@

tools/%: tools/%.c $(LIBRARY) $(HEADERS)
	$(CC) $(CFLAGS) -Itests $< $(LIBRARY) -o $@

tools: $(TOOLS)

# Golden vectors are committed; this regenerates them. Review any diff: a
# changed vector means a changed bitstream.
vectors: tools/mzg2_vectors
	mkdir -p $(VECTOR_DIR)
	./tools/mzg2_vectors $(VECTOR_DIR)

test: $(TESTS)
	@status=0; \
	for binary in $(TESTS); do \
		echo "== $$binary"; \
		./$$binary || status=1; \
	done; \
	exit $$status

# The HIP decoder. Compiling needs ROCm; running needs a wave32 GPU.
hip: core/src/decode_hip.o

core/src/decode_hip.o: core/src/decode_hip.hip
	$(HIPCC) $(HIPFLAGS) -c $< -o $@

clean:
	rm -f $(LIB_OBJECTS) $(LIBRARY) $(TESTS) $(TOOLS) core/src/decode_hip.o
