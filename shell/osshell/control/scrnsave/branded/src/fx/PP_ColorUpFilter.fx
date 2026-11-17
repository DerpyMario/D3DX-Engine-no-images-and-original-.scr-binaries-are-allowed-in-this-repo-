//------------------------------------------------------------------------------
// File: PP_ColorUpFilter.fx
//------------------------------------------------------------------------------


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


//------------------------------------------------------------------------------
// Pixel Shader: UpFilterPS
// Desc: Performs upfiltering to scale the image to the original size.
//------------------------------------------------------------------------------
float4 UpFilterPS(float2 Tex : TEXCOORD0) : COLOR0 {
    return tex2D(g_samSrcColor, Tex);
}


//------------------------------------------------------------------------------
// Technique: PostProcess
// Desc: Performs post-processing effect that down-filters.
//------------------------------------------------------------------------------
technique PostProcess {
    pass p0 
    <
        float fScaleX = 4.0f;
        float fScaleY = 4.0f;
    >
    {
        ZEnable = false;

        VertexShader = null;
        PixelShader = compile ps_2_0 UpFilterPS();
    }
}
