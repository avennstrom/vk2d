TARGET_NAME = game
CC = gcc
C_FILES := $(wildcard src/*.c)
C_OBJ := $(patsubst src/%.c,obj/%.o,$(C_FILES))
H_FILES := $(wildcard src/*.h)

SHADER_OBJ := ${SHADER_OBJ} obj/world.vs.spo obj/world.fs.spo
SHADER_OBJ := ${SHADER_OBJ} obj/model.vs.spo obj/model.fs.spo
SHADER_OBJ := ${SHADER_OBJ} obj/debug.vs.spo obj/debug.fs.spo
SHADER_OBJ := ${SHADER_OBJ} obj/composite.vs.spo obj/composite.fs.spo

.PHONY: shaderc run-converter
.SECONDARY: $(SHADER_SPV)

default: shaderc ${TARGET_NAME} converter run-converter

${TARGET_NAME}: ${C_OBJ} ${SHADER_OBJ} | $(SHADER_DIS) ${H_FILES}
	$(CC) -Werror -std=c11 -L${VULKAN_SDK}/lib -g -o $@ $^ -lvulkan -lX11 -lm

obj:
	@mkdir -p obj

obj/%.o: src/%.c | obj
	$(CC) -std=c11 -I${VULKAN_SDK}/include -Werror -g -MMD -MF $@.d -c -o $@ $<

obj/%.spv: shaders/%
	glslangValidator -V -o $@ $^

%.spv.s: %.spv
	spirv-dis -o $@ $^

obj/%.spo: obj/%.spv
	@ld -z noexecstack -r -b binary -o $@ $^

shaderc:
	./compile_shaders.sh

run-converter: converter
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

-include $(C_OBJ:.o=.o.d)