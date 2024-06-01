#include "mat.h"

#include <math.h>

mat4 mat_identity(void)
{
	return (mat4){
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1},
	};
}

mat4 mat_mul(mat4 lhs, mat4 rhs)
{
	mat4 out;

	out.r0.x = lhs.r0.x * rhs.r0.x + lhs.r0.y * rhs.r1.x + lhs.r0.z * rhs.r2.x + lhs.r0.w * rhs.r3.x;
	out.r0.y = lhs.r0.x * rhs.r0.y + lhs.r0.y * rhs.r1.y + lhs.r0.z * rhs.r2.y + lhs.r0.w * rhs.r3.y;
	out.r0.z = lhs.r0.x * rhs.r0.z + lhs.r0.y * rhs.r1.z + lhs.r0.z * rhs.r2.z + lhs.r0.w * rhs.r3.z;
	out.r0.w = lhs.r0.x * rhs.r0.w + lhs.r0.y * rhs.r1.w + lhs.r0.z * rhs.r2.w + lhs.r0.w * rhs.r3.w;

	out.r1.x = lhs.r1.x * rhs.r0.x + lhs.r1.y * rhs.r1.x + lhs.r1.z * rhs.r2.x + lhs.r1.w * rhs.r3.x;
	out.r1.y = lhs.r1.x * rhs.r0.y + lhs.r1.y * rhs.r1.y + lhs.r1.z * rhs.r2.y + lhs.r1.w * rhs.r3.y;
	out.r1.z = lhs.r1.x * rhs.r0.z + lhs.r1.y * rhs.r1.z + lhs.r1.z * rhs.r2.z + lhs.r1.w * rhs.r3.z;
	out.r1.w = lhs.r1.x * rhs.r0.w + lhs.r1.y * rhs.r1.w + lhs.r1.z * rhs.r2.w + lhs.r1.w * rhs.r3.w;

	out.r2.x = lhs.r2.x * rhs.r0.x + lhs.r2.y * rhs.r1.x + lhs.r2.z * rhs.r2.x + lhs.r2.w * rhs.r3.x;
	out.r2.y = lhs.r2.x * rhs.r0.y + lhs.r2.y * rhs.r1.y + lhs.r2.z * rhs.r2.y + lhs.r2.w * rhs.r3.y;
	out.r2.z = lhs.r2.x * rhs.r0.z + lhs.r2.y * rhs.r1.z + lhs.r2.z * rhs.r2.z + lhs.r2.w * rhs.r3.z;
	out.r2.w = lhs.r2.x * rhs.r0.w + lhs.r2.y * rhs.r1.w + lhs.r2.z * rhs.r2.w + lhs.r2.w * rhs.r3.w;

	out.r3.x = lhs.r3.x * rhs.r0.x + lhs.r3.y * rhs.r1.x + lhs.r3.z * rhs.r2.x + lhs.r3.w * rhs.r3.x;
	out.r3.y = lhs.r3.x * rhs.r0.y + lhs.r3.y * rhs.r1.y + lhs.r3.z * rhs.r2.y + lhs.r3.w * rhs.r3.y;
	out.r3.z = lhs.r3.x * rhs.r0.z + lhs.r3.y * rhs.r1.z + lhs.r3.z * rhs.r2.z + lhs.r3.w * rhs.r3.z;
	out.r3.w = lhs.r3.x * rhs.r0.w + lhs.r3.y * rhs.r1.w + lhs.r3.z * rhs.r2.w + lhs.r3.w * rhs.r3.w;

	return out;
}

mat4 mat_transpose(mat4 m)
{
	return (mat4){
		{m.r0.x, m.r1.x, m.r2.x, m.r3.x},
		{m.r0.y, m.r1.y, m.r2.y, m.r3.y},
		{m.r0.z, m.r1.z, m.r2.z, m.r3.z},
		{m.r0.w, m.r1.w, m.r2.w, m.r3.w},
	};
}

float determinant(mat4 m)
{
	float a1 = m.r0.x * (m.r1.y * (m.r2.z * m.r3.w - m.r3.z * m.r2.w) - m.r1.z * (m.r2.y * m.r3.w - m.r3.y * m.r2.w) + m.r1.w * (m.r2.y * m.r3.z - m.r3.y * m.r2.z));
	float a2 = -m.r0.y * (m.r1.x * (m.r2.z * m.r3.w - m.r3.z * m.r2.w) - m.r1.z * (m.r2.x * m.r3.w - m.r3.x * m.r2.w) + m.r1.w * (m.r2.x * m.r3.z - m.r3.x * m.r2.z));
	float a3 = m.r0.z * (m.r1.x * (m.r2.y * m.r3.w - m.r3.y * m.r2.w) - m.r1.y * (m.r2.x * m.r3.w - m.r3.x * m.r2.w) + m.r1.w * (m.r2.x * m.r3.y - m.r3.x * m.r2.y));
	float a4 = -m.r0.w * (m.r1.x * (m.r2.y * m.r3.z - m.r3.y * m.r2.z) - m.r1.y * (m.r2.x * m.r3.z - m.r3.x * m.r2.z) + m.r1.z * (m.r2.x * m.r3.y - m.r3.x * m.r2.y));

	return a1 + a2 + a3 + a4;
}

mat4 mat_adjoint(mat4 m)
{
	mat4 adj;

	adj.r0.x = m.r1.y * (m.r2.z * m.r3.w - m.r3.z * m.r2.w) - m.r1.z * (m.r2.y * m.r3.w - m.r3.y * m.r2.w) + m.r1.w * (m.r2.y * m.r3.z - m.r3.y * m.r2.z);
	adj.r0.y = -(m.r0.y * (m.r2.z * m.r3.w - m.r3.z * m.r2.w) - m.r0.z * (m.r2.y * m.r3.w - m.r3.y * m.r2.w) + m.r0.w * (m.r2.y * m.r3.z - m.r3.y * m.r2.z));
	adj.r0.z = m.r0.y * (m.r1.z * m.r3.w - m.r3.z * m.r1.w) - m.r0.z * (m.r1.y * m.r3.w - m.r3.y * m.r1.w) + m.r0.w * (m.r1.y * m.r3.z - m.r3.y * m.r1.z);
	adj.r0.w = -(m.r0.y * (m.r1.z * m.r2.w - m.r2.z * m.r1.w) - m.r0.z * (m.r1.y * m.r2.w - m.r2.y * m.r1.w) + m.r0.w * (m.r1.y * m.r2.z - m.r2.y * m.r1.z));

	adj.r1.x = -(m.r1.x * (m.r2.z * m.r3.w - m.r3.z * m.r2.w) - m.r1.z * (m.r2.x * m.r3.w - m.r3.x * m.r2.w) + m.r1.w * (m.r2.x * m.r3.z - m.r3.x * m.r2.z));
	adj.r1.y = m.r0.x * (m.r2.z * m.r3.w - m.r3.z * m.r2.w) - m.r0.z * (m.r2.x * m.r3.w - m.r3.x * m.r2.w) + m.r0.w * (m.r2.x * m.r3.z - m.r3.x * m.r2.z);
	adj.r1.z = -(m.r0.x * (m.r1.z * m.r3.w - m.r3.z * m.r1.w) - m.r0.z * (m.r1.x * m.r3.w - m.r3.x * m.r1.w) + m.r0.w * (m.r1.x * m.r3.z - m.r3.x * m.r1.z));
	adj.r1.w = m.r0.x * (m.r1.z * m.r2.w - m.r2.z * m.r1.w) - m.r0.z * (m.r1.x * m.r2.w - m.r2.x * m.r1.w) + m.r0.w * (m.r1.x * m.r2.z - m.r2.x * m.r1.z);

	adj.r2.x = m.r1.x * (m.r2.y * m.r3.w - m.r3.y * m.r2.w) - m.r1.y * (m.r2.x * m.r3.w - m.r3.x * m.r2.w) + m.r1.w * (m.r2.x * m.r3.y - m.r3.x * m.r2.y);
	adj.r2.y = -(m.r0.x * (m.r2.y * m.r3.w - m.r3.y * m.r2.w) - m.r0.y * (m.r2.x * m.r3.w - m.r3.x * m.r2.w) + m.r0.w * (m.r2.x * m.r3.y - m.r3.x * m.r2.y));
	adj.r2.z = m.r0.x * (m.r1.y * m.r3.w - m.r3.y * m.r1.w) - m.r0.y * (m.r1.x * m.r3.w - m.r3.x * m.r1.w) + m.r0.w * (m.r1.x * m.r3.y - m.r3.x * m.r1.y);
	adj.r2.w = -(m.r0.x * (m.r1.y * m.r2.w - m.r2.y * m.r1.w) - m.r0.y * (m.r1.x * m.r2.w - m.r2.x * m.r1.w) + m.r0.w * (m.r1.x * m.r2.y - m.r2.x * m.r1.y));

	adj.r3.x = -(m.r1.x * (m.r2.y * m.r3.z - m.r3.y * m.r2.z) - m.r1.y * (m.r2.x * m.r3.z - m.r3.x * m.r2.z) + m.r1.z * (m.r2.x * m.r3.y - m.r3.x * m.r2.y));
	adj.r3.y = m.r0.x * (m.r2.y * m.r3.z - m.r3.y * m.r2.z) - m.r0.y * (m.r2.x * m.r3.z - m.r3.x * m.r2.z) + m.r0.z * (m.r2.x * m.r3.y - m.r3.x * m.r2.y);
	adj.r3.z = -(m.r0.x * (m.r1.y * m.r3.z - m.r3.y * m.r1.z) - m.r0.y * (m.r1.x * m.r3.z - m.r3.x * m.r1.z) + m.r0.z * (m.r1.x * m.r3.y - m.r3.x * m.r1.y));
	adj.r3.w = m.r0.x * (m.r1.y * m.r2.z - m.r2.y * m.r1.z) - m.r0.y * (m.r1.x * m.r2.z - m.r2.x * m.r1.z) + m.r0.z * (m.r1.x * m.r2.y - m.r2.x * m.r1.y);

	return adj;
}

mat4 mat_invert(mat4 m)
{
	float det = determinant(m);
	if (det == 0.0f)
	{
		// Matrix is singular, cannot invert
		// Return the original matrix
		return m;
	}

	float invDet = 1.0f / det;
	mat4 adj = mat_adjoint(m);

	mat4 result;
	result.r0.x = adj.r0.x * invDet;
	result.r0.y = adj.r0.y * invDet;
	result.r0.z = adj.r0.z * invDet;
	result.r0.w = adj.r0.w * invDet;

	result.r1.x = adj.r1.x * invDet;
	result.r1.y = adj.r1.y * invDet;
	result.r1.z = adj.r1.z * invDet;
	result.r1.w = adj.r1.w * invDet;

	result.r2.x = adj.r2.x * invDet;
	result.r2.y = adj.r2.y * invDet;
	result.r2.z = adj.r2.z * invDet;
	result.r2.w = adj.r2.w * invDet;

	result.r3.x = adj.r3.x * invDet;
	result.r3.y = adj.r3.y * invDet;
	result.r3.z = adj.r3.z * invDet;
	result.r3.w = adj.r3.w * invDet;

	return result;
}

mat4 mat_translate(mat4 m, vec3 pos)
{
	const mat4 result = {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{pos.x, pos.y, pos.z, 1}};

	return mat_mul(result, m);
}

mat4 mat_rotate_x(mat4 m, float rad)
{
	float cosRad = cos(rad);
	float sinRad = sin(rad);

	const mat4 result = {
		{1, 0, 0, 0},
		{0, cosRad, -sinRad, 0},
		{0, sinRad, cosRad, 0},
		{0, 0, 0, 1}};

	return mat_mul(result, m);
}

mat4 mat_rotate_y(mat4 m, float rad)
{
	const float cosRad = cos(rad);
	const float sinRad = sin(rad);

	const mat4 result = {
		{cosRad, 0, sinRad, 0},
		{0, 1, 0, 0},
		{-sinRad, 0, cosRad, 0},
		{0, 0, 0, 1}};

	return mat_mul(result, m);
}

mat4 mat_rotate_z(mat4 m, float rad)
{
	const float cosRad = cos(rad);
	const float sinRad = sin(rad);

	const mat4 result = {
		{cosRad, -sinRad, 0, 0},
		{sinRad, cosRad, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1}};

	return mat_mul(result, m);
}

mat4 mat_perspective(float fovY, float aspect, float near, float far)
{
	float f = 1.0f / tan(fovY / 2.0f * (3.14159265358979323846f / 180.0f));

	return (mat4){
		{f / aspect, 0, 0, 0},
		{0, f, 0, 0},
		{0, 0, (far + near) / (near - far), -1},
		{0, 0, (2.f * far * near) / (near - far), 0},
	};
}