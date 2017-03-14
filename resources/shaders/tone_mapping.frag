#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding=0, input_attachment_index=0) uniform subpassInput inColor;
layout(location=0) out vec4 outColor;

// optimized formula by Jim Hejl and Richard Burgess-Dawson.
vec3 toneMapHejl(vec3 linearColor, float exposure) {
    //from comment section at http://filmicgames.com/archives/75
    //The 0.004 sets the value for the black point to give you a little more
    //contrast in the bottom end. The graph will look very close, you will see a
    //noticeable difference in the blacks in your images because the human eye
    //has more precision in the darker areas.
    vec3 x = max(vec3(0), fma(linearColor, vec3(exposure), -vec3(0.004)));
    return (x * (6.2 * x + 0.5)) / ( x * (6.2 * x + vec3(1.7)) + vec3(0.06));
}

const float A = 0.15; // ShoulderStrength
const float B = 0.50; // LinearStrength
const float C = 0.10; // LinearAngle
const float D = 0.20; // ToeStrength
const float E = 0.02; // ToeNumerator
const float F = 0.30; // ToeDenominator
const float W = 11.2; // LinearWhite

vec3 Uncharted2Tonemap(vec3 linearColor)
{
  return ((linearColor*(A*linearColor+C*B)+D*E)/(linearColor*(A*linearColor+B)+D*F))-E/F;
}

void main() {
    vec3 color = subpassLoad(inColor).rgb;
    float exposure = 1.0;
#if 1
    vec3 curr = Uncharted2Tonemap(exposure * color);
    vec3 whiteScale = 1.0f / Uncharted2Tonemap(vec3(W));
    vec3 col = pow(curr*whiteScale, vec3(1.0/2.2));
#else
   vec3 col = toneMapHejl(color, exposure);
#endif

    // outColor = vec4(color, 1.0); // passthrough
    outColor = vec4(col, 1.0);
}
