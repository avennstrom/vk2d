#!/bin/bash

mkdir -p dat

./modelc -o dat/cube.model -i content/cube.glb
./modelc -o dat/sphere.model -i content/sphere.glb