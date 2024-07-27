#!/bin/bash

mkdir -p dat

#./modelc -o dat/cube.model -i content/cube.glb 
#./modelc -o dat/sphere.model -i content/sphere.glb
./modelc -o dat/tank.model -i content/tank.glb -r Tank
./modelc -o dat/colortest.model -i content/colortest.glb -r Cube