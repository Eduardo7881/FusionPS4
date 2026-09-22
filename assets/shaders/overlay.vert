#version 450
layout(push_constant) uniform Push {
    float screenWidth;
    float screenHeight;
} pc;
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;
layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;
void main() {
    // Convert pixel coordinates to NDC.
    vec2 ndc = vec2(
        (inPos.x / pc.screenWidth) * 2.0 - 1.0,
        (inPos.y / pc.screenHeight) * 2.0 - 1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    outUV = inUV;
    outColor = inColor;
}
