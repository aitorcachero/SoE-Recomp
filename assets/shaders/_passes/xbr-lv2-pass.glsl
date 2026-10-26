// xBR-lv2 Edge-Smoothing Shader for Retro Pixel Art
// Preserves clean edges and rounds off stair-stepping with directional interpolation.

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

float colorDist(vec3 c1, vec3 c2) {
    vec3 d = abs(c1 - c2);
    return dot(d, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 pos = vTexCoord * TextureSize;
    vec2 p = floor(pos);
    vec2 f = fract(pos);
    vec2 invSize = 1.0 / TextureSize;

    // Sample 3x3 neighborhood around central texel
    vec3 A = texture(Texture, (p + vec2(-0.5, -0.5)) * invSize).rgb;
    vec3 B = texture(Texture, (p + vec2( 0.5, -0.5)) * invSize).rgb;
    vec3 C = texture(Texture, (p + vec2( 1.5, -0.5)) * invSize).rgb;
    vec3 D = texture(Texture, (p + vec2(-0.5,  0.5)) * invSize).rgb;
    vec3 E = texture(Texture, (p + vec2( 0.5,  0.5)) * invSize).rgb; // Center
    vec3 F = texture(Texture, (p + vec2( 1.5,  0.5)) * invSize).rgb;
    vec3 G = texture(Texture, (p + vec2(-0.5,  1.5)) * invSize).rgb;
    vec3 H = texture(Texture, (p + vec2( 0.5,  1.5)) * invSize).rgb;
    vec3 I = texture(Texture, (p + vec2( 1.5,  1.5)) * invSize).rgb;

    vec3 res = E;

    // Bottom-Right quadrant
    if (f.x > 0.5 && f.y > 0.5) {
        float d_diag = colorDist(D, H) + colorDist(B, F);
        float d_opp  = colorDist(E, I) * 2.0;
        if (d_diag < d_opp) {
            float t = clamp((f.x + f.y - 1.0) * 1.5, 0.0, 1.0);
            vec3 blendCol = (colorDist(E, H) < colorDist(E, F)) ? H : F;
            res = mix(E, blendCol, t * 0.75);
        }
    }
    // Top-Right quadrant
    else if (f.x > 0.5 && f.y <= 0.5) {
        float d_diag = colorDist(D, B) + colorDist(H, F);
        float d_opp  = colorDist(E, C) * 2.0;
        if (d_diag < d_opp) {
            float t = clamp((f.x + (1.0 - f.y) - 1.0) * 1.5, 0.0, 1.0);
            vec3 blendCol = (colorDist(E, B) < colorDist(E, F)) ? B : F;
            res = mix(E, blendCol, t * 0.75);
        }
    }
    // Bottom-Left quadrant
    else if (f.x <= 0.5 && f.y > 0.5) {
        float d_diag = colorDist(B, D) + colorDist(F, H);
        float d_opp  = colorDist(E, G) * 2.0;
        if (d_diag < d_opp) {
            float t = clamp(((1.0 - f.x) + f.y - 1.0) * 1.5, 0.0, 1.0);
            vec3 blendCol = (colorDist(E, D) < colorDist(E, H)) ? D : H;
            res = mix(E, blendCol, t * 0.75);
        }
    }
    // Top-Left quadrant
    else {
        float d_diag = colorDist(F, B) + colorDist(H, D);
        float d_opp  = colorDist(E, A) * 2.0;
        if (d_diag < d_opp) {
            float t = clamp(((1.0 - f.x) + (1.0 - f.y) - 1.0) * 1.5, 0.0, 1.0);
            vec3 blendCol = (colorDist(E, B) < colorDist(E, D)) ? B : D;
            res = mix(E, blendCol, t * 0.75);
        }
    }

    // Vibrance polish
    float luma = dot(res, vec3(0.299, 0.587, 0.114));
    float maxCol = max(max(res.r, res.g), res.b);
    float minCol = min(min(res.r, res.g), res.b);
    res = mix(vec3(luma), res, 1.0 + 0.15 * (1.0 - (maxCol - minCol)));

    FragColor = vec4(clamp(res, 0.0, 1.0), 1.0);
}
#endif
