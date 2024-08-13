#!/bin/bash

mkdir -p obj

dxc -D__HLSL__ -Fo obj/world.vs.spv -T vs_6_8 -spirv shaders/world.hlsl -E vs_main
dxc -D__HLSL__ -Fo obj/world.fs.spv -T ps_6_8 -spirv shaders/world.hlsl -E fs_main
ld -z noexecstack -r -b binary -o obj/world.vs.spo obj/world.vs.spv
ld -z noexecstack -r -b binary -o obj/world.fs.spo obj/world.fs.spv

dxc -D__HLSL__ -Fo obj/model.vs.spv -T vs_6_8 -spirv shaders/model.hlsl -E vs_main
dxc -D__HLSL__ -Fo obj/model.fs.spv -T ps_6_8 -spirv shaders/model.hlsl -E fs_main
ld -z noexecstack -r -b binary -o obj/model.vs.spo obj/model.vs.spv
ld -z noexecstack -r -b binary -o obj/model.fs.spo obj/model.fs.spv

dxc -D__HLSL__ -Fo obj/debug.vs.spv -T vs_6_8 -spirv shaders/debug.hlsl -E vs_main
dxc -D__HLSL__ -Fo obj/debug.fs.spv -T ps_6_8 -spirv shaders/debug.hlsl -E fs_main
ld -z noexecstack -r -b binary -o obj/debug.vs.spo obj/debug.vs.spv
ld -z noexecstack -r -b binary -o obj/debug.fs.spo obj/debug.fs.spv

dxc -D__HLSL__ -Fo obj/composite.vs.spv -T vs_6_8 -spirv shaders/composite.hlsl -E vs_main
dxc -D__HLSL__ -Fo obj/composite.fs.spv -T ps_6_8 -spirv shaders/composite.hlsl -E fs_main
ld -z noexecstack -r -b binary -o obj/composite.vs.spo obj/composite.vs.spv
ld -z noexecstack -r -b binary -o obj/composite.fs.spo obj/composite.fs.spv

dxc -D__HLSL__ -Fo obj/particle.vs.spv -T vs_6_8 -spirv shaders/particle.hlsl -E vs_main
dxc -D__HLSL__ -Fo obj/particle.fs.spv -T ps_6_8 -spirv shaders/particle.hlsl -E fs_main
ld -z noexecstack -r -b binary -o obj/particle.vs.spo obj/particle.vs.spv
ld -z noexecstack -r -b binary -o obj/particle.fs.spo obj/particle.fs.spv