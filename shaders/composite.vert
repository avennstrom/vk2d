#version 450

layout(location = 0) out vec2 uv;

void main()
{
	const uint i = gl_VertexIndex % 3;
	if (i == 0) {
		uv = vec2(0, 0);
	}
	else if (i == 1) {
		uv = vec2(2, 0);
	}
	else {
		uv = vec2(0, 2);
	}

	gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}