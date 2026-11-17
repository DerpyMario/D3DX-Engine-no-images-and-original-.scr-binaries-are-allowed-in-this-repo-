

// global variables
float4x3 g_mWorldView  : WORLDVIEW;
float4x4 g_mProj : PROJECTION;
texture  g_tTexture0;


// local constants
float4 cMatAmb  = { 0.2f, 0.2f, 0.2f, 1.0f };
float4 cMatDiff = { 0.8f, 0.8f, 0.8f, 1.0f };


sampler FlareSamp = sampler_state {
    Texture = <g_tTexture0>;
    AddressU = Wrap;
    AddressV = Wrap;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
};


struct VS_OUTPUT
{
    float4 vVPos : POSITION;
    float2 vTex  : TEXCOORD0;
    float4 cDiff : COLOR0;
};


VS_OUTPUT FlareVS(float3 vWPos  : POSITION,
                  float3 vWNorm : NORMAL,
                  float2 vTex : TEXCOORD0)
{
    VS_OUTPUT Out = (VS_OUTPUT)0;

    float3 vVPos, vVNorm, vVToEye, vVHalf;
    float4 cAmb, cDiff, cSpec;

    vVPos = mul(float4(vWPos, 1), (float4x3)g_mWorldView);    // position (view space)
    vVNorm = normalize(mul(vWNorm, (float3x3)g_mWorldView));  // normal (view space)
                                                              
    cAmb  = cMatAmb ;
    cDiff = cMatDiff * -vVNorm.z;                  // Light is always behind camera

    Out.vVPos = mul(float4(vVPos, 1), g_mProj);    // position (screen space)
    Out.vTex = vTex;
    Out.cDiff = cDiff + cAmb;                      // diffuse + ambient
//    Out.cDiff.a = 1.0;

    return Out;
}


struct PS_OUTPUT {
  float4 cPixel : COLOR0;
};


PS_OUTPUT FlarePS(VS_OUTPUT In) { 
  PS_OUTPUT Output;

  Output.cPixel = In.cDiff * tex2D(FlareSamp, In.vTex);
//  Output.cPixel.a = 0.0;

  return Output;
}


technique Render
{
    pass P0
    {
//        AlphaBlendEnable = False;
//        BlendOp = Max;
//        SrcBlend = SrcAlpha;
//        DestBlend = One;

        AlphaBlendEnable = True;
        BlendOp = Add;
        SrcBlend = SrcAlpha;
        DestBlend = InvSrcAlpha;

        SpecularEnable = False;
        CullMode = CCW;
        ZEnable = True;

        VertexShader = compile vs_2_0 FlareVS();
        PixelShader = compile ps_2_0 FlarePS();
    }
}

