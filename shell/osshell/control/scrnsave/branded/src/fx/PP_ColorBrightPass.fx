//-----------------------------------------------------------------------------
// File: PP_ColorBrightPass.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
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


float Luminance = 0.08f;
static const float fMiddleGray = 0.18f;
static const float fWhiteCutoff = 0.8f;

//-----------------------------------------------------------------------------
// Pixel Shader: BrightPassFilter
// Desc: Perform a high-pass filter on the source texture
//-----------------------------------------------------------------------------
float4 BrightPassFilter( in float2 Tex : TEXCOORD0 ) : COLOR0 {
    float3 ColorOut = tex2D( g_samSrcColor, Tex );

    ColorOut *= fMiddleGray / ( Luminance + 0.001f );
    ColorOut *= ( 1.0f + ( ColorOut / ( fWhiteCutoff * fWhiteCutoff ) ) );
    ColorOut -= 5.0f;

    ColorOut = max( ColorOut, 0.0f );

    ColorOut /= ( 10.0f + ColorOut );

    return float4( ColorOut, 1.0f );
}


//-----------------------------------------------------------------------------
// Technique: PostProcess
// Desc: Performs post-processing effect that converts a colored image to
//       black and white.
//-----------------------------------------------------------------------------
technique PostProcess
<
    string Parameter0 = "Luminance";
    float4 Parameter0Def = float4( 0.08f, 0, 0, 0 );
    int Parameter0Size = 1;
    string Parameter0Desc = " (float)";
>
{
    pass p0 {
        VertexShader = null;
        PixelShader = compile ps_2_0 BrightPassFilter();
        ZEnable = false;
    }
}
