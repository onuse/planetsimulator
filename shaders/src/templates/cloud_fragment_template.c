// Cloud layer fragment shader.
//
// Transpiled to shaders/src/fragment/cloud.frag and compiled to
// cloud.frag.spv.
//
// The simulation says how much of the sky is covered at each point, on a grid
// seventeen kilometres across. That is the right scale for where weather is
// and the wrong scale for what a cloud looks like, so the cover decides how
// much cloud there is and noise decides its shape - the same division as
// terrain relief, where the simulation places the mountains and the noise
// gives them texture.

// GLSL_BEGIN
#version 450

layout(location = 0) in float fragCover;
layout(location = 1) in vec3 fragUp;
layout(location = 2) in vec3 fragViewDir;
layout(location = 3) in float fragEyeDistance;

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 viewPos;
    float time;
    vec3 lightDir;
    float pixelWorldScale;
    vec4 planetParams;
} ubo;

layout(location = 0) out vec4 outColor;

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

float valueNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float n000 = hash13(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash13(i + vec3(1.0, 1.0, 1.0));

    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

void main() {
    // Weather is only shown when time is running slowly enough for a still
    // sky to be honest. See planetParams.z, set from the simulation rate.
    float weather = ubo.planetParams.z;

    // Nothing to draw over most of a planet, and the sooner that is known the
    // better - a clear sky should cost one texture-free branch.
    if (fragCover * weather <= 0.01) {
        discard;
    }

    // Cloud shape. Several octaves, so the edges are ragged at every scale
    // rather than smooth blobs at one.
    float shape = 0.0;
    float amplitude = 0.5;
    float frequency = 40.0;
    for (int octave = 0; octave < 5; octave++) {
        shape += valueNoise(fragUp * frequency) * amplitude;
        frequency *= 2.3;
        amplitude *= 0.55;
    }

    // Cover sets where the threshold sits, so a sky the model calls half
    // covered has cloud over about half of it. Thresholding rather than
    // multiplying is what gives clouds edges instead of a uniform haze.
    float threshold = 1.0 - fragCover;
    float density = smoothstep(threshold - 0.12, threshold + 0.18, shape);
    if (density <= 0.003) {
        discard;
    }

    // Lit from the side the sun is on, with enough light coming through to
    // keep the shadowed side grey rather than black - cloud scatters far more
    // than it absorbs, which is why an overcast day is bright.
    vec3 sunDir = normalize(-ubo.lightDir);
    float sunlight = smoothstep(-0.25, 0.35, dot(fragUp, sunDir));

    vec3 lit = mix(vec3(0.34, 0.38, 0.46), vec3(1.0, 0.98, 0.95), sunlight);

    // Thicker cloud is brighter where the sun reaches it and darker where it
    // does not, because there is more of it to do either.
    lit *= mix(0.82, 1.0, density);

    // Fade out at a grazing angle. A shell has no thickness, so where the line
    // of sight runs along it the cloud would otherwise present a hard edge
    // against space rather than thinning away.
    float grazing = abs(dot(normalize(fragUp), normalize(fragViewDir)));
    float edge = smoothstep(0.0, 0.30, grazing);

    outColor = vec4(lit, density * edge * weather * 0.92);
}
// GLSL_END
