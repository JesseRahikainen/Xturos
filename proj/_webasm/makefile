NUKLEAR_DIR = F:/Data/Libraries/nuklear-master
SPINE_DIR = F:/Data/Libraries/spine-runtimes-master/spine-c
STB_DIR = F:/Data/Libraries/stb-master

CSRC = $(wildcard ../../src/*.c) \
       $(wildcard ../../src/Graphics/*.c) \
       $(wildcard ../../src/IMGUI/*.c) \
	   $(wildcard ../../src/Input/*.c) \
	   $(wildcard ../../src/Math/*.c) \
	   $(wildcard ../../src/System/*.c) \
	   $(wildcard ../../src/System/ECPS/*.c) \
	   $(wildcard ../../src/UI/*.c) \
	   $(wildcard ../../src/Utils/*.c) \
	   $(wildcard ../../src/Components/*.c) \
	   $(wildcard ../../src/Processes/*.c) \
	   $(wildcard ../../src/Game/*.c) \
	   $(wildcard $(SPINE_DIR)/src/spine/*.c)

OBJS = $(CSRC:.c=.o)

CC = emcc

CFLAGS = -s WASM=1 \
		 -s USE_SDL=2 \
		 -s USE_WEBGL2=1 \
		 -s FULL_ES3=1 \
		 -s ALLOW_MEMORY_GROWTH=1 \
		 -I$(NUKLEAR_DIR) \
		 -I$(STB_DIR) \
		 -I$(SPINE_DIR)/include \
		 --emrun \
		 --preload-file ../../bin/data@/ \
		 --no-heap-copy
		 
OUT = Xturos.html
RBUILD = release_build
DBUILD = debug_build

release : $(OBJS)
	rm -rf $(RBUILD)
	mkdir $(RBUILD)
	$(CC) $(CFLAGS) -O3 $(OBJS) -o $(RBUILD)/$(OUT)

debug : $(OBJS)
	rm -rf $(DBUILD)
	mkdir $(DBUILD)
	$(CC) $(CFLAGS) -g4 -v -s ASSERTIONS=2 -s SAFE_HEAP_LOG=1 -s STACK_OVERFLOW_CHECK=1 $(OBJS) -o $(DBUILD)/$(OUT)

test:
	emrun --no_browser --port 8080 .

clean_objs:
	rm -f $(OBJS)

clean:
	rm -f $(OBJS)
	rm -rf $(RBUILD)
	rm -rf $(DBUILD)