// Fast Approximate Anti-Aliasing (FXAA) for Retro Games
// Smooths high-contrast stair-steps and edge aliasing.

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

float rgb2luma(vec3 rgb) {
    return dot(rgb, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 rcpFrame = 1.0 / TextureSize;
    vec3 rgbM = texture(Texture, vTexCoord).rgb;
    float lumaM = rgb2luma(rgbM);

    vec3 rgbNW = texture(Texture, vTexCoord + vec2(-rcpFrame.x, -rcpFrame.y)).rgb;
    vec3 rgbNE = texture(Texture, vTexCoord + vec2( rcpFrame.x, -rcpFrame.y)).rgb;
    vec3 rgbSW = texture(Texture, vTexCoord + vec2(-rcpFrame.x,  rcpFrame.y)).rgb;
    vec3 rgbSE = texture(Texture, vTexCoord + vec2( rcpFrame.x,  rcpFrame.y)).rgb;

    float lumaNW = rgb2luma(rgbNW);
    float lumaNE = rgb2luma(rgbNE);
    float lumaSW = rgb2luma(rgbSW);
    float lumaSE = rgb2luma(rgbSE);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float range = lumaMax - lumaMin;

    if (range < max(0.05, lumaMax * 0.125)) {
        FragColor = vec4(rgbM, 1.0);
        return;
    }

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * 0.0625, 0.0078125);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(8.0), max(vec2(-8.0), dir * rcpDirMin)) * rcpFrame;

    vec3 rgbA = 0.5 * (
        texture(Texture, vTexCoord + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(Texture, vTexCoord + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(Texture, vTexCoord + dir * -0.5).rgb +
        texture(Texture, vTexCoord + dir * 0.5).rgb);

    float lumaB = rgb2luma(rgbB);
    vec3 finalColor = ((lumaB < lumaMin) || (lumaB > lumaMax)) ? rgbA : rgbB;

    FragColor = vec4(finalColor, 1.0);
}
#endif
