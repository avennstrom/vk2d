TARGET_NAME = game
CC = gcc
C_FILES := $(wildcard src/*.c)
C_OBJ := $(patsubst src/%.c,obj/%.o,$(C_FILES))
C_OBJ += obj/profiler.o
H_FILES := $(wildcard src/*.h)

SHADER_OBJ := ${SHADER_OBJ} obj/world.vs.spo obj/world.fs.spo
SHADER_OBJ := ${SHADER_OBJ} obj/model.vs.spo obj/model.fs.spo
SHADER_OBJ := ${SHADER_OBJ} obj/debug.vs.spo obj/debug.fs.spo
SHADER_OBJ := ${SHADER_OBJ} obj/composite.vs.spo obj/composite.fs.spo
SHADER_OBJ := ${SHADER_OBJ} obj/particle.vs.spo obj/particle.fs.spo

CC_DEFINES = -D_POSIX_C_SOURCE=200809L
CC_DEFINES += -DTRACY_ENABLE

.PHONY: shaderc
.SECONDARY: $(SHADER_SPV)

default: shaderc ${TARGET_NAME}

${TARGET_NAME}: obj/tracyclient.o ${C_OBJ} ${SHADER_OBJ} | $(SHADER_DIS) ${H_FILES}
	g++ -Werror -std=c11 -L${VULKAN_SDK}/lib -g -o $@ $^ -lvulkan -lX11 -lm

obj:
	@mkdir -p obj

obj/%.o: src/%.c | obj
	$(CC) -std=c11 ${CC_DEFINES} -I${VULKAN_SDK}/include -Werror -g -MMD -MF $@.d -c -o $@ $<

obj/profiler.o: src/profiler.cpp | obj
	$(CC) ${CC_DEFINES} -Itracy/public -Werror -g -MMD -MF $@.d -c -o $@ $<

obj/tracyclient.o: tracy/public/TracyClient.cpp
	g++ ${CC_DEFINES} -c -o $@ $<

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