#!/usr/bin/python3

import os
import subprocess
import yaml

shaderdir = 'shaders'

pipelines = {}

def get_spirv_path(glsl):
	basename = os.path.basename(glsl)
	return 'obj/%s.spv' % (basename)

def get_reflect_path(glsl):
	basename = os.path.basename(glsl)
	return 'obj/%s.yaml' % (basename)

def compile(glsl):
	cmd = 'glslangValidator --quiet -V -o %s %s' % (get_spirv_path(glsl), glsl)
	#print(cmd)
	print(glsl)
	subprocess.run(cmd, shell=True)

def reflect(glsl):
	cmd = 'spirv-reflect -y -v 0 %s' % (get_spirv_path(glsl))
	#print(cmd)
	result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
	# with open(get_reflect_path(glsl), 'w') as f:
	# 	f.write(result.stdout)
	#print(result.stdout)
	lines = result.stdout.split('\n')
	asd = '\n'.join(lines[2:])
	#print(asd)
	return yaml.load(asd, Loader=yaml.CLoader)

for filename in os.listdir('shaders'):
	file_path = os.path.join(shaderdir, filename)
	noext, ext = os.path.splitext(filename)
	if ext not in ['.frag', '.vert', '.geom']: continue
	#print(basename)
	if noext in pipelines:
		pipelines[noext].append(file_path)
	else:
		pipelines[noext] = [file_path]

BindingType_UniformBuffer = 2
BindingType_StorageBuffer = 4
BindingType_CombinedImageSampler = 5

bindingTypeDesc = {
	BindingType_UniformBuffer: 'uniform',
	BindingType_StorageBuffer: 'storage',
	BindingType_CombinedImageSampler: 'combinedImageSampler',
}

for basename, files in pipelines.items():
	#print('Compiling pipeline ' + basename)
	for f in files:
		compile(f)

	refl = []
	for f in files:
		refl.append(reflect(f))
		
	bindings = refl[0]['all_descriptor_bindings']
	if not bindings:
		continue

	for b in bindings:
		print('(set=%s, binding=%s) %s %s' % (b['set'], b['binding'], bindingTypeDesc[b['resource_type']], b['name']))