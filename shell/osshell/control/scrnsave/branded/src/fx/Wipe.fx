//------------------------------------------------------------------------------
// File: Wipe.fx
//
// Blends Src0 with Src1, according to Threshold and a WipeMap.
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Global variables
//------------------------------------------------------------------------------
float   g_fLevel0;
float   g_fLevel1;
float   g_fWidth;

texture g_tSrc0;
float3  g_cColor0;
bool    g_bSolidColor0;

texture g_tSrc1;
float3  g_cColor1;
bool    g_bSolidColor1;

texture g_tWipeMap;


//------------------------------------------------------------------------------
// Texture samplers
//------------------------------------------------------------------------------
sampler Src0Sampler = sampler_state {
    Texture = <g_tSrc0>;
    AddressU = Clamp;
    AddressV = Clamp;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
};

sampler Src1Sampler = sampler_state {
    Texture = <g_tSrc1>;
    AddressU = Clamp;
    AddressV = Clamp;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
};

sampler WipeSampler = sampler_state {
    Texture = <g_tWipeMap>;
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
// shader WipeVS
//
// Computes standard transform and lighting
//------------------------------------------------------------------------------
VS_OUTPUT WipeVS(float4 vPos : POSITION,
                 float2 vTex : TEXCOORD0) {
  VS_OUTPUT Output;

  Output.Position = vPos;
  Output.Tex      = vTex;

  return Output;  
}


//------------------------------------------------------------------------------
// WipePS
//------------------------------------------------------------------------------
float4 WipePS(VS_OUTPUT In) : COLOR0 { 
  float fMapValue = tex2D(WipeSampler, In.Tex).r;  // Only using red channel for now
  float fWipeValue = smoothstep(g_fLevel0, g_fLevel1, fMapValue);

  float4 cSrc0 = float4(g_cColor0, 1.0);
  float4 cSrc1 = float4(g_cColor1, 1.0);

  if(!g_bSolidColor0) {
    cSrc0 = tex2D(Src0Sampler, In.Tex);
  }
  if(!g_bSolidColor1) {
    cSrc1 = tex2D(Src1Sampler, In.Tex);
  }

  float4 cResult;
  cResult = (cSrc1 * fWipeValue) + (cSrc0 * (1.0 - fWipeValue));
  cResult.a = 1;

  return cResult;
}


//------------------------------------------------------------------------------
// Render things
//------------------------------------------------------------------------------

technique Render {
  pass P0 {
    ZEnable = false;

    VertexShader = compile vs_1_1 WipeVS();
    PixelShader  = compile ps_2_0 WipePS();
  }
}
