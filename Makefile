TARGET_NAME = rts
CC = gcc
C_FILES := $(wildcard src/*.c)
C_OBJ := $(patsubst src/%.c,obj/%.o,$(C_FILES))
H_FILES := $(wildcard src/*.h)

SHADER_FILES := $(wildcard shaders/*.vert shaders/*.frag shaders/*.geom shaders/*.comp)
SHADER_SPV := $(patsubst shaders/%,obj/%.spv,$(SHADER_FILES))
SHADER_DIS := $(patsubst shaders/%,obj/%.spv.s,$(SHADER_FILES))
SHADER_OBJ := $(patsubst shaders/%,obj/%.spo,$(SHADER_FILES))

SHADER_OBJ := ${SHADER_OBJ} obj/model.vs.spo obj/model.fs.spo
SHADER_OBJ := ${SHADER_OBJ} obj/terrain.vs.spo obj/terrain.fs.spo

.PHONY: shaderc run-converter
.SECONDARY: $(SHADER_SPV)

default: shaderc ${TARGET_NAME} converter run-converter

${TARGET_NAME}: ${C_OBJ} ${SHADER_OBJ} | $(SHADER_DIS) ${H_FILES}
	$(CC) -Werror -std=c11 -L${VULKAN_SDK}/lib -g -o $@ $^ -lvulkan -lX11 -lm

obj:
	@mkdir -p obj

obj/%.o: src/%.c | obj
	$(CC) -Werror -std=c11 -I${VULKAN_SDK}/include -g -c -o $@ $^

obj/%.spv: shaders/%
	glslangValidator -V -o $@ $^

%.spv.s: %.spv
	spirv-dis -o $@ $^

obj/%.spo: obj/%.spv
	@ld -z noexecstack -r -b binary -o $@ $^

shaderc:
	./compile_shaders.sh

run-converter:
	./converter

converter: src/converter/main.c src/converter/gltf_parser.c src/converter/glb_parser.c
	$(CC) -Wall -Wshadow -Werror -std=c11 -g -o $@ $^

clean:
	rm -f ${TARGET_NAME}
	rm -f converter
	rm -rf obj
	rm -rf dat

debug:
	@echo $(SHADER_OBJ)
	@echo $(C_FILES)
	@echo $(C_OBJ)