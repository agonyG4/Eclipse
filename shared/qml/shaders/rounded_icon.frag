#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float roundedRadius;
    float presentationMode;
    vec4 tintColor;
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
    const float alpha = pixel.a;
    const float epsilon = 0.00001;
    vec3 presented = alpha > epsilon ? pixel.rgb : vec3(0.0);
    if (alpha > epsilon && presentationMode > 0.5) {
        const vec3 straightRgb = pixel.rgb / alpha;
        const float luma = dot(straightRgb, vec3(0.2126, 0.7152, 0.0722));
        vec3 straightPresented;
        if (presentationMode < 1.5) {
            straightPresented = vec3(luma);
        } else {
            const vec3 straightTint = tintColor.a > epsilon
                ? tintColor.rgb / tintColor.a : vec3(0.0);
            const vec3 darkAccent = straightTint * 0.24;
            straightPresented = mix(darkAccent, straightTint, clamp(luma, 0.0, 1.0));
        }
        presented = straightPresented * alpha;
    }
    fragColor = vec4(presented * coverage, pixel.a * coverage) * qt_Opacity;
}
