//------------------------------------------------------------------------------
// File: Aurora.fx
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Common variables
//------------------------------------------------------------------------------
float4x4 g_mWorldViewProj;
texture g_tTexture0;
float g_fAmp0;
float g_fFreq0;
float g_fPhase0;
float g_fAmp1;
float g_fFreq1;
float g_fPhase1;
float g_fAmp2;
float g_fFreq2;
float g_fPhase2;
float g_fWid;
float g_fHgt;


//------------------------------------------------------------------------------
// Texture samplers
//------------------------------------------------------------------------------
sampler AuroraSampler = sampler_state {
    Texture = <g_tTexture0>;
    AddressU = Wrap;
    AddressV = Wrap;
    MipFilter = None;
    MinFilter = Linear;
    MagFilter = Linear;
};


//------------------------------------------------------------------------------
// Vertex shader output structure
//------------------------------------------------------------------------------
struct VS_OUTPUT {
  float4 vPos      : POSITION;  // vertex position 
  float4 cColor    : COLOR0;    // only alpha used
  float2 vTex      : TEXCOORD0; // vertex texture coordinate
};


//------------------------------------------------------------------------------
// shader AuroraVS
//
// Computes standard transform and lighting
//------------------------------------------------------------------------------
VS_OUTPUT AuroraVS(float4 vPos : POSITION,
                   float2 vTex : TEXCOORD0) {
  VS_OUTPUT Output;
  
  // This is where the aurora really gets generated.  The input is a flat
  // sheet.

  float fSin0 = sin((vPos.x + g_fPhase0) * g_fFreq0);
  float fSin1 = sin((vPos.x + g_fPhase1) * g_fFreq1);
  float fSin2 = sin((vPos.x + g_fPhase2) * g_fFreq2);
  vPos.z = g_fAmp0 * fSin0 + g_fAmp1 * fSin1 + g_fAmp2 * fSin2;

  float fCos0 = cos((vPos.x + g_fPhase0) * g_fFreq0);
  float fCos1 = cos((vPos.x + g_fPhase1) * g_fFreq1);
  float fCos2 = cos((vPos.x + g_fPhase2) * g_fFreq2);

  float fComp0 = g_fAmp0 * fCos0;
  float fComp1 = g_fAmp1 * fCos1;
  float fComp2 = g_fAmp2 * fCos2;

  float3 vTan, vNorm;
  vTan.x = 1.0;
  vTan.y = 0.0;
  vTan.z = fComp0 + fComp1 + fComp2;
  vTan = normalize(vTan);
  vNorm.x = vTan.z;
  vNorm.y = 0.0;
  vNorm.z = -vTan.x;

  float fFalloff = cos(abs(vPos.x) * 3.14);
  fFalloff *= fFalloff; // Falloff ^ 2
  fFalloff *= fFalloff; // Falloff ^ 4

  vPos.x = vPos.x * g_fWid;
  vPos.y = vPos.y * g_fHgt;
  Output.vPos = mul(vPos, g_mWorldViewProj);

  Output.cColor = 1.0;
  Output.cColor.a = abs(vNorm.z) * fFalloff * 0.45;

  Output.vTex = vTex;
  
  return Output;  
}


//------------------------------------------------------------------------------
// Pixel shader output structure
//------------------------------------------------------------------------------
struct PS_OUTPUT {
  float4 cPixel : COLOR0;  // Pixel color  
};


//------------------------------------------------------------------------------
// AuroraPS
//------------------------------------------------------------------------------
PS_OUTPUT AuroraPS(VS_OUTPUT In) { 
  PS_OUTPUT Output;

  Output.cPixel = In.cColor;
  Output.cPixel.a = In.cColor.a * tex2D(AuroraSampler, In.vTex).a;

  return Output;
}


//------------------------------------------------------------------------------
// Render things
//------------------------------------------------------------------------------

technique Render {
  pass P0 {
    AlphaBlendEnable = True;
    BlendOp = Add;
    SrcBlend = SrcAlpha;
    DestBlend = InvSrcAlpha;

    ZEnable = false;
    CullMode = None;
    SpecularEnable = false;

    VertexShader = compile vs_2_0 AuroraVS();
    PixelShader  = compile ps_2_0 AuroraPS();
  }
}











































