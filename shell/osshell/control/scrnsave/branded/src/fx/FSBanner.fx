//------------------------------------------------------------------------------
// File: FSBanner.fx
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Global variables
//------------------------------------------------------------------------------
texture g_tTexture0;


//------------------------------------------------------------------------------
// Texture samplers
//------------------------------------------------------------------------------
sampler FSBannerSampler = sampler_state {
    Texture = <g_tTexture0>;
    AddressU = Clamp;
    AddressV = Clamp;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
};


//------------------------------------------------------------------------------
// Vertex shader output structure
//------------------------------------------------------------------------------
struct VS_OUTPUT {
  float4 Position : POSITION;  // vertex position
  float2 Tex      : TEXCOORD0; // vertex texture coordinate
};


//------------------------------------------------------------------------------
// shader FSBannerVS
//
// Computes standard transform and lighting
//------------------------------------------------------------------------------
VS_OUTPUT FSBannerVS(float4 vPos : POSITION,
                 float2 vTex : TEXCOORD0) {
  VS_OUTPUT Output;

  Output.Position = vPos;
  Output.Tex      = vTex;

  return Output;  
}


//------------------------------------------------------------------------------
// FSBannerPS
//------------------------------------------------------------------------------
float4 FSBannerPS(VS_OUTPUT In) : COLOR0 { 
  return tex2D(FSBannerSampler, In.Tex);
}


//------------------------------------------------------------------------------
// Render things
//------------------------------------------------------------------------------

technique Render {
  pass P0 {
    ZEnable = false;

    VertexShader = compile vs_1_1 FSBannerVS();
    PixelShader = compile ps_1_1 FSBannerPS();
  }
}
