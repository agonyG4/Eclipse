#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float roundedRadius;
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    const vec2 center = vec2(0.5);
    const float radius = clamp(roundedRadius, 0.0, 0.5);
    const vec2 halfExtent = vec2(0.5 - radius);
    const vec2 q = abs(qt_TexCoord0 - center) - halfExtent;
    const float signedDistance = length(max(q, vec2(0.0)))
        + min(max(q.x, q.y), 0.0) - radius;
    const float antialiasWidth = max(fwidth(signedDistance), 1.0 / 1024.0);
    const float coverage = 1.0 - smoothstep(-antialiasWidth,
                                             antialiasWidth,
                                             signedDistance);
    const vec4 pixel = texture(source, qt_TexCoord0);
    fragColor = vec4(pixel.rgb * coverage, pixel.a * coverage) * qt_Opacity;
}
