//-----------------------------------------------------------------------------
// File: PP_ColorCombine4.fx
//-----------------------------------------------------------------------------


texture g_txSrcColor;
texture g_txSceneColor;


sampler2D g_samSrcColor = sampler_state {
    Texture = <g_txSrcColor>;
    AddressU = Clamp;
    AddressV = Clamp;
    MinFilter = Point;
    MagFilter = Linear;
    MipFilter = Linear;
};

sampler2D g_samSceneColor = sampler_state {
    Texture = <g_txSceneColor>;
    AddressU = Clamp;
    AddressV = Clamp;
    MinFilter = Point;
    MagFilter = Linear;
    MipFilter = Linear;
};


//-----------------------------------------------------------------------------
// Pixel Shader: Combine
// Desc: Combine the source image with the original image.
//-----------------------------------------------------------------------------
float4 Combine(float2 Tex : TEXCOORD0,
               float2 Tex2 : TEXCOORD1) : COLOR0 {
    float3 ColorOrig = tex2D(g_samSceneColor, Tex2);

//DBG    ColorOrig += tex2D(g_samSrcColor, Tex);
    ColorOrig = (0.5 * ColorOrig) + (0.8 * tex2D(g_samSrcColor, Tex));

    return float4(ColorOrig, 1.0f);
}


//-----------------------------------------------------------------------------
// Technique: PostProcess
// Desc: Performs post-processing effect that down-filters.
//-----------------------------------------------------------------------------
technique PostProcess {
    pass p0
    <
        float fScaleX = 4.0f;
        float fScaleY = 4.0f;
    >
    {
        ZEnable = false;

        VertexShader = null;
        PixelShader = compile ps_2_0 Combine();
    }
}
