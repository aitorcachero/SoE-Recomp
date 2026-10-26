// HD-2D Remaster Shader for Secret of Evermore
// Sub-pixel edge anti-aliasing, micro-contrast enhancement, and dynamic color vibrance.

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

vec3 sampleColor(vec2 uv) {
    return texture(Texture, clamp(uv, vec2(0.0), vec2(1.0))).rgb;
}

void main() {
    vec2 texel = 1.0 / TextureSize;
    vec3 c = sampleColor(vTexCoord);

    // 1. Edge-directed anti-aliasing and micro-contrast
    vec3 up    = sampleColor(vTexCoord + vec2(0.0, -texel.y));
    vec3 down  = sampleColor(vTexCoord + vec2(0.0,  texel.y));
    vec3 left  = sampleColor(vTexCoord + vec2(-texel.x, 0.0));
    vec3 right = sampleColor(vTexCoord + vec2( texel.x, 0.0));
    vec3 diag1 = sampleColor(vTexCoord + vec2(-texel.x, -texel.y));
    vec3 diag2 = sampleColor(vTexCoord + vec2( texel.x,  texel.y));
    vec3 diag3 = sampleColor(vTexCoord + vec2(-texel.x,  texel.y));
    vec3 diag4 = sampleColor(vTexCoord + vec2( texel.x, -texel.y));

    // High frequency laplacian
    vec3 laplace = 8.0 * c - (up + down + left + right + diag1 + diag2 + diag3 + diag4);
    c = clamp(c + 0.14 * laplace, 0.0, 1.0);

    // 2. Dynamic range and contrast curve
    c = (c - 0.5) * 1.08 + 0.5;
    c = clamp(c, 0.0, 1.0);

    // 3. Selective Color Vibrance (expands 15-bit SNES palette to vibrant modern 24-bit)
    float luma = dot(c, vec3(0.299, 0.587, 0.114));
    float maxCol = max(max(c.r, c.g), c.b);
    float minCol = min(min(c.r, c.g), c.b);
    float sat = maxCol - minCol;
    float vibrance = 0.28;
    c = mix(vec3(luma), c, 1.0 + vibrance * (1.0 - sat));

    FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
#endif
