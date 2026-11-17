//-----------------------------------------------------------------------------
// File: PP_ColorGBlurV.fx
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

//static const int g_cKernelSize = 13;
//
//float2 PixelKernel[g_cKernelSize] = {
//    { 0, -6 },
//    { 0, -5 },
//    { 0, -4 },
//    { 0, -3 },
//    { 0, -2 },
//    { 0, -1 },
//    { 0,  0 },
//    { 0,  1 },
//    { 0,  2 },
//    { 0,  3 },
//    { 0,  4 },
//    { 0,  5 },
//    { 0,  6 },
//};
//
//static const float BlurWeights[g_cKernelSize] = {
//    0.002216,
//    0.008764,
//    0.026995,
//    0.064759,
//    0.120985,
//    0.176033,
//    0.199471,
//    0.176033,
//    0.120985,
//    0.064759,
//    0.026995,
//    0.008764,
//    0.002216,
//};

static const int g_cKernelSize = 7;

float2 PixelKernel[g_cKernelSize] = {
    { 0, -3 },
    { 0, -2 },
    { 0, -1 },
    { 0,  0 },
    { 0,  1 },
    { 0,  2 },
    { 0,  3 },
};

static const float BlurWeights[g_cKernelSize] = {
    0.018,
    0.089,
    0.232,
    0.319,
    0.232,
    0.089,
    0.018,
};

float2 TexelKernel[g_cKernelSize]
<
    string ConvertPixelsToTexels = "PixelKernel";
>;

float BloomScale = 1.5f;


//-----------------------------------------------------------------------------
// Pixel Shader: HorizontalBlur
// Desc: Blurs the image vertically
//-----------------------------------------------------------------------------
float4 PostProcessPS( float2 Tex : TEXCOORD0 ) : COLOR0 {
    float4 Color = 0;

    for (int i=0; i<g_cKernelSize; i++) {    
        Color += tex2D( g_samSrcColor, Tex + TexelKernel[i].xy ) * BlurWeights[i];
    }

    return Color * BloomScale;
}




//-----------------------------------------------------------------------------
// Technique: PostProcess
// Desc: Performs post-processing effect that converts a colored image to
//       black and white.
//-----------------------------------------------------------------------------
technique PostProcess
<
    string Parameter0 = "BloomScale";
    float4 Parameter0Def = float4( 1.5f, 0, 0, 0 );
    int Parameter0Size = 1;
    string Parameter0Desc = " (float)";
>
{
    pass p0 {
        VertexShader = null;
        PixelShader = compile ps_2_0 PostProcessPS();
        ZEnable = false;
    }
}
