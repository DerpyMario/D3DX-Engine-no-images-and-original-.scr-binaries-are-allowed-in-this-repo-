//------------------------------------------------------------------------------
// File: LogoGlow.fx
//------------------------------------------------------------------------------


float4x4 g_mWorldViewProj;
texture  g_tTexture0;
texture  g_tTexture1;
float    g_fFrac;


//------------------------------------------------------------------------------
// Texture samplers
//------------------------------------------------------------------------------
sampler LGSamp0 = sampler_state {
    Texture = <g_tTexture0>;
    AddressU = Wrap;
    AddressV = Wrap;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
};

sampler LGSamp1 = sampler_state {
    Texture = <g_tTexture1>;
    AddressU = Wrap;
    AddressV = Wrap;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
};


//------------------------------------------------------------------------------
// Vertex shader output structure
//------------------------------------------------------------------------------
struct VS_OUTPUT {
    float4 vPos : POSITION;  // vertex position
    float2 vTex : TEXCOORD0; // vertex texture coordinate
};


//------------------------------------------------------------------------------
// SimpleVS
//------------------------------------------------------------------------------
VS_OUTPUT SimpleVS(float4 vPos : POSITION,
                   float2 vTex : TEXCOORD0) {
    VS_OUTPUT Output;

    Output.vPos = mul(vPos, g_mWorldViewProj);
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
// SimplePS
//------------------------------------------------------------------------------
PS_OUTPUT SimplePS(VS_OUTPUT In) { 
  PS_OUTPUT Output;

  Output.cPixel = tex2D(LGSamp0, In.vTex) * (1.0 - g_fFrac) + tex2D(LGSamp1, In.vTex) * g_fFrac;

  return Output;
}


//------------------------------------------------------------------------------
// Techniques
//------------------------------------------------------------------------------
technique Render {
    pass p0 {
        AlphaBlendEnable = True;
        BlendOp = Add;
        SrcBlend = SrcAlpha;
        DestBlend = One;
    
        ZEnable = True;
    
        VertexShader = compile vs_1_1 SimpleVS();
        PixelShader = compile ps_2_0 SimplePS();
    }
}


