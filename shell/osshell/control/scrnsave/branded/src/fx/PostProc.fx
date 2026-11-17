//------------------------------------------------------------------------------
// File: PostProc.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//------------------------------------------------------------------------------


texture g_txSrc;
float3 g_cTargetColor;


sampler2D g_samSrc = sampler_state {
  Texture = <g_txSrc>;
  AddressU = Clamp;
  AddressV = Clamp;
  MinFilter = Linear;
  MagFilter = Linear;
  MipFilter = Linear;
};


//------------------------------------------------------------------------------
// PostProcPS()
//
// Purpose:
//   Outputs texture value without lighting calculation.  Used for fullscreen
//   quad copy.
//------------------------------------------------------------------------------
float4 PostProcPS(float2 Tex : TEXCOORD0) : COLOR {
  float4 cPostProc;
  float4 cFinal;

  cPostProc = tex2D(g_samSrc, Tex);

  float fAlpha = max(cPostProc.r, max(cPostProc.g, cPostProc.b));

  cFinal.rgb = g_cTargetColor * 0.5 + cPostProc.rgb * 0.95;
//  cFinal.a = 0.65;
  cFinal.a = 0.50;

  return cFinal;
}


technique Render {
  pass p0 {
    AlphaBlendEnable = True;
    BlendOp = Add;
    SrcBlend = SrcAlpha;
    DestBlend = One;

    ZEnable = False;

    VertexShader = null;
    PixelShader = compile ps_2_0 PostProcPS();
  }
}


