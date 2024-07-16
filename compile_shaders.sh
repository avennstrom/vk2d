#!/bin/bash

mkdir -p obj

dxc -D__HLSL__ -Fo obj/model.vs.spv -T vs_6_8 -spirv shaders/model.hlsl -E vs_main
dxc -D__HLSL__ -Fo obj/model.fs.spv -T ps_6_8 -spirv shaders/model.hlsl -E fs_main

ld -z noexecstack -r -b binary -o obj/model.vs.spo obj/model.vs.spv
ld -z noexecstack -r -b binary -o obj/model.fs.spo obj/model.fs.spv