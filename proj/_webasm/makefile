NUKLEAR_DIR = F:/Data/Libraries/nuklear-master
SPINE_DIR = F:/Data/Libraries/spine-runtimes-master/spine-c
STB_DIR = F:/Data/Libraries/stb-master

CSRC = $(wildcard ../../src/Game/*.c) \
       $(wildcard ../../src/Game/Graphics/*.c) \
	   $(wildcard ../../src/Game/Graphics/Platform/*.c) \
	   $(wildcard ../../src/Game/Graphics/Platform/OpenGL/*.c) \
       $(wildcard ../../src/Game/IMGUI/*.c) \
	   $(wildcard ../../src/Game/Input/*.c) \
	   $(wildcard ../../src/Game/Math/*.c) \
	   $(wildcard ../../src/Game/System/*.c) \
	   $(wildcard ../../src/Game/System/ECPS/*.c) \
	   $(wildcard ../../src/Game/UI/*.c) \
	   $(wildcard ../../src/Game/Utils/*.c) \
	   $(wildcard ../../src/Game/Components/*.c) \
	   $(wildcard ../../src/Game/Processes/*.c) \
	   $(wildcard ../../src/Game/Game/*.c) \
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
		 -I../../src/Game \
		 --emrun \
		 --preload-file ../../bin/data@/ \
		 --no-heap-copy \
		 -DOPENGL_GFX \
		 -DNK_INCLUDE_STANDARD_IO
		 
OUT = index.html
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