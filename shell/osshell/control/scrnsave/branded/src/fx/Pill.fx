

// global variables
float4x3 g_mWorldView  : WORLDVIEW;
float4x4 g_mProj : PROJECTION;
float3   g_vScale;
texture  g_tTexture0;


sampler PillSampler = sampler_state {
    Texture = <g_tTexture0>;
    AddressU = Clamp;
    AddressV = Clamp;
    MipFilter = None;
    MinFilter = Linear;
    MagFilter = Linear;
};


struct VS_OUTPUT
{
    float4 vVPos : POSITION;
    float2 vTex  : TEXCOORD0;
};


VS_OUTPUT VS(float3 vWPos  : POSITION, 
             float3 vWNorm : NORMAL,
             float2 vTex   : TEXCOORD0)
{
    VS_OUTPUT Out = (VS_OUTPUT)0;

    vWPos *= g_vScale;
    float3 vVPos;

    vVPos = mul(float4(vWPos, 1), (float4x3)g_mWorldView);    // position (view space)
                                                              
    Out.vVPos = mul(float4(vVPos, 1), g_mProj);               // position (screen space)
    Out.vTex = vTex;

    return Out;
}


struct PS_OUTPUT {
  float4 cPixel : COLOR0;  // Pixel color  
};


PS_OUTPUT PS(VS_OUTPUT In) { 
  PS_OUTPUT Output;

  Output.cPixel = tex2D(PillSampler, In.vTex);

  return Output;
}


technique Render
{
    pass P0
    {
        AlphaBlendEnable = True;
        BlendOp = Max;
        SrcBlend = One;
        DestBlend = One;

        SpecularEnable = False;
        CullMode = CCW;
        ZEnable = False;

        VertexShader = compile vs_2_0 VS();
        PixelShader = compile ps_2_0 PS();
    }
}

