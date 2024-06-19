#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 vert;
layout(location = 1) in vec3 normal;

layout(set = 1, binding = 0) uniform FragmentBuffer {
	vec3 lightPos;
	float constantFalloff;
	vec3 lightColor;
	float linearFalloff;
	float quadraticFalloff;
} lightBuffer;

void main() {
	vec3 toLight = lightBuffer.lightPos - vert; 
	float distToL = length(toLight);
	
	float linearFalloff = distToL * lightBuffer.linearFalloff;
	float quadraticFalloff = distToL * lightBuffer.quadraticFalloff;
	float constant = lightBuffer.constantFalloff;

	float attenuation = 1 / (constant + linearFalloff + quadraticFalloff);

	float lightIntensity = attenuation * max(0.0f, dot(normal, normalize(toLight)));

	vec4 lightColor = vec4(lightBuffer.lightColor, 1.0f);
	vec4 ambient = lightColor * 0.1f;

	outColor = (lightIntensity * lightColor) + ambient;
}