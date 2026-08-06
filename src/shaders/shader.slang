// Square covering entire screen
static float2 positions[6] = float2[](
    // top-left triangle
    float2(-1.0, -1.0),
    float2(1.0, -1.0),
    float2(-1.0, 1.0),
    // bottom-right triangle
    float2(1.0, -1.0),
    float2(1.0, 1.0),
    float2(-1.0, 1.0)
);

struct VertexOutput{
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexOutput vertMain(uint vid : SV_VertexID) {
    VertexOutput output;
    output.sv_position = float4(positions[vid], 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target {
    return float4(1.0, 0.0, 0.0, 1.0);
}
