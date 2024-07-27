#pragma once

#include "types.h"

mat4 mat_identity(void);
mat4 mat_mul(mat4 lhs, mat4 rhs);
mat4 mat_transpose(mat4 m);
mat4 mat_invert(mat4 m);

mat4 mat_translate(mat4 m, vec3 pos);
mat4 mat_rotate_x(mat4 m, float rad);
mat4 mat_rotate_y(mat4 m, float rad);
mat4 mat_rotate_z(mat4 m, float rad);
mat4 mat_rotate(mat4 m, vec4 q);

mat4 mat_perspective(float fovY, float aspect, float near, float far);
