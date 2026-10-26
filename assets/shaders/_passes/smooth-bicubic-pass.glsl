// Smooth Bicubic Catmull-Rom Shader for Retro Games
// High quality smooth texture scaling without blocky pixels.

#ifdef VERTEX
in vec2 VertexCoord;
in vec2 TexCoord;
out vec2 vTexCoord;

void main() {
    vTexCoord = TexCoord;
    gl_Position = vec4(VertexCoord * 2.0 - 1.0, 0.0, 1.0);
}
#endif

#ifdef FRAGMENT
uniform sampler2D Texture;
uniform vec2 TextureSize;
in vec2 vTexCoord;
out vec4 FragColor;

vec4 cubic(float v) {
    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
    vec4 s = n * n * n;
    float x = s.x;
    float y = s.y - 4.0 * s.x;
    float z = s.z - 4.0 * s.y + 6.0 * s.x;
    float w = 6.0 - x - y - z;
    return vec4(x, y, z, w) * (1.0 / 6.0);
}

void main() {
    vec2 texSize = TextureSize;
    vec2 invTexSize = 1.0 / texSize;

    vec2 coord = vTexCoord * texSize - 0.5;
    vec2 f = fract(coord);
    vec2 index = floor(coord);

    vec4 xcubic = cubic(f.x);
    vec4 ycubic = cubic(f.y);

    vec4 c = vec4(index.x - 0.5, index.x + 1.5, index.y - 0.5, index.y + 1.5);
    vec4 s = vec4(xcubic.x + xcubic.y, xcubic.z + xcubic.w, ycubic.x + ycubic.y, ycubic.z + ycubic.w);
    vec4 offset = c + vec4(xcubic.y, xcubic.w, ycubic.y, ycubic.w) / s;

    vec4 sample0 = texture(Texture, vec2(offset.x, offset.z) * invTexSize);
    vec4 sample1 = texture(Texture, vec2(offset.y, offset.z) * invTexSize);
    vec4 sample2 = texture(Texture, vec2(offset.x, offset.w) * invTexSize);
    vec4 sample3 = texture(Texture, vec2(offset.y, offset.w) * invTexSize);

    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);

    vec4 color = mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
    FragColor = vec4(clamp(color.rgb, 0.0, 1.0), 1.0);
}
#endif
