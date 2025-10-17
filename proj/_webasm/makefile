STB_DIR = F:/Data/Libraries/stb-master
SDL_DIR = F:/Data/Libraries/SDL3-3.2.0
SDL_LIB = SDL3
SDL_LIB_DIR = $(SDL_DIR)/em-build
SDL_INC = $(SDL_DIR)/include

FILTERED_FILES = ../../src/Game/Others/lua-5.4.3/luac.c

SRC_DIR = ../../src/Game
DATA_DIR = ../../bin/data

CSRC = $(wildcard $(SRC_DIR)/*.c) \
       $(wildcard $(SRC_DIR)/Audio/*.c) \
       $(wildcard $(SRC_DIR)/Graphics/*.c) \
	   $(wildcard $(SRC_DIR)/Graphics/Platform/*.c) \
	   $(wildcard $(SRC_DIR)/Graphics/Platform/OpenGL/*.c) \
       $(wildcard $(SRC_DIR)/IMGUI/*.c) \
	   $(wildcard $(SRC_DIR)/IMGUI/Platform/OpenGL/*.c) \
	   $(wildcard $(SRC_DIR)/Input/*.c) \
	   $(wildcard $(SRC_DIR)/Math/*.c) \
	   $(wildcard $(SRC_DIR)/System/*.c) \
	   $(wildcard $(SRC_DIR)/System/ECPS/*.c) \
	   $(wildcard $(SRC_DIR)/UI/*.c) \
	   $(wildcard $(SRC_DIR)/Utils/*.c) \
	   $(wildcard $(SRC_DIR)/Components/*.c) \
	   $(wildcard $(SRC_DIR)/Processes/*.c) \
	   $(wildcard $(SRC_DIR)/Game/*.c) \
	   $(wildcard $(SRC_DIR)/Editors/*.c) \
	   $(wildcard $(SRC_DIR)/Others/cmp.c) \
	   $(wildcard $(SRC_DIR)/DefaultECPS/*.c) \
	   $(wildcard $(SRC_DIR)/Others/lua-5.4.3/*.c)

CSRC := $(filter-out $(FILTERED_FILES), $(CSRC))

OBJS = $(CSRC:.c=.o)

CC = emcc

CFLAGS = -s WASM=1 \
		 -s USE_WEBGL2=1 \
		 -s FULL_ES3=1 \
		 -s ALLOW_MEMORY_GROWTH=1 \
		 -I$(STB_DIR) \
		 -I$(SDL_INC) \
		 -I../../src/Game \
		 --emrun \
		 --preload-file $(DATA_DIR)@/ \
		 --no-heap-copy \
		 -DOPENGL_GFX \
		 -DNK_INCLUDE_STANDARD_IO \
		 -L$(SDL_LIB_DIR) \
		 -l$(SDL_LIB) \
		 -DSCRIPTING_ENABLED
		 
OUT = index.html
RBUILD = release_build
DBUILD = debug_build

release: $(OBJS)
	rm -rf $(RBUILD)
	mkdir $(RBUILD)
	rm -rf $(DATA_DIR)/Shaders
	cp -R $(SRC_DIR)/Shaders/GLES $(DATA_DIR)/Shaders
	$(CC) $(CFLAGS) -O3 $(OBJS) -o $(RBUILD)/$(OUT)

debug: $(OBJS)
	rm -rf $(DBUILD)
	mkdir $(DBUILD)
	rm -rf $(DATA_DIR)/Shaders
	cp -R $(SRC_DIR)/Shaders/GLES $(DATA_DIR)/Shaders
	$(CC) $(CFLAGS) -gsource-map -v -s ASSERTIONS=2 -s SAFE_HEAP_LOG=1 -s STACK_OVERFLOW_CHECK=1 $(OBJS) -o $(DBUILD)/$(OUT)

test:
	$(info At http://127.0.0.1:8080/)
	emrun --no_browser --port 8080 .

clean_objs:
	rm -f $(OBJS)

clean:
	rm -f $(OBJS)
	rm -rf $(RBUILD)
	rm -rf $(DBUILD)