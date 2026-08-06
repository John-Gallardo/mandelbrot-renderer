// Square covering entire screen
static float2 g_positions[6] = float2[](
    // top-left triangle
    float2(-1.0, -1.0),
    float2(1.0, -1.0),
    float2(-1.0, 1.0),
    // bottom-right triangle
    float2(1.0, -1.0),
    float2(1.0, 1.0),
    float2(-1.0, 1.0)
);

static int g_maxIterations = 1000;
static int g_divergenceBoundary = 2;

int calculateDepth(float real, float imag) {
    // NOTE: set initially to z0's values
    float realZn = 0, imagZn = 0;
    int depth = 0;

    for (int i = 0; i < g_maxIterations; i++) {
        // 1. calculate real & imaginary for zn+1
        
        // zn+1 = zn^2 + c = (-(imagZn^2) + realZn^2 + real) + i(2*realZn*imagZn + imag)
        // ^ derive using complex math
        realZn = -(imagZn * imagZn) + realZn * realZn + real;
        imagZn = 2*realZn*imagZn + imag;
        
        // 2. check for divergence
        float znDistance = sqrt(realZn * realZn + imagZn * imagZn);
        if (znDistance >= g_divergenceBoundary)
            break;
        
        // 3. increment depth & go to next zn+1
        depth++;
    }

    return depth;
}

struct VertexOutput{
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexOutput vertMain(uint vid : SV_VertexID) {
    VertexOutput output;
    output.sv_position = float4(g_positions[vid], 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target {
    float real = inVert.sv_position.x, imaginary = -inVert.sv_position.y;  // NOTE: negative to flip, since negative y is up on Vulkan
    float depth = calculateDepth(real, imaginary);
    
    // color = white for convergence
    if (depth == g_maxIterations) 
        return float4(1.0, 1.0, 1.0, 1.0);

    float alpha = 1 / depth;
    return float4(alpha * float3(0.0, 1.0, 0.5), 1.0);
}
