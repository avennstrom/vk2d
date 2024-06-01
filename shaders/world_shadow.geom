#version 460

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

void main()
{
	const int lightIndex = int(gl_in[0].gl_PointSize);

	gl_Layer = lightIndex;
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
	gl_Layer = lightIndex;
	gl_Position = gl_in[1].gl_Position;
	EmitVertex();
	gl_Layer = lightIndex;
	gl_Position = gl_in[2].gl_Position;
	EmitVertex();

	EndPrimitive();
}