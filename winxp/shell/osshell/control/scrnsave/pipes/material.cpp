//-----------------------------------------------------------------------------
// File: material.cpp
//
// Desc: The materials of the pipes: 24 classic materials (ambient, diffuse
//       and specular color and shininess) for solid pipes, and 4 light ones
//       for textured pipes.
//
//       Reconstructed from material.obj:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\material.obj
//-----------------------------------------------------------------------------
#include "stdafx.h"

// The materials that look good on pipes
int goodMaterials[NUM_GOOD_MATERIALS] =
{
    0, 1, 3, 4, 5, 6, 7, 9, 10, 11, 13, 16, 17, 19, 20, 22
};

// Ambient rgb, diffuse rgb, specular rgb, shininess (fraction of 128)
static FLOAT teaMaterialData[NUM_TEA_MATERIALS][10] =
{
    { 0.0215f, 0.1745f, 0.0215f, 0.07568f, 0.61424f, 0.07568f, 0.633f, 0.727811f, 0.633f, 0.6f },   // emerald
    { 0.135f, 0.2225f, 0.1575f, 0.54f, 0.89f, 0.63f, 0.316228f, 0.316228f, 0.316228f, 0.1f },   // jade
    { 0.05375f, 0.05f, 0.06625f, 0.18275f, 0.17f, 0.22525f, 0.332741f, 0.328634f, 0.346435f, 0.3f },   // obsidian
    { 0.25f, 0.20725f, 0.20725f, 1.0f, 0.829f, 0.829f, 0.296648f, 0.296648f, 0.296648f, 0.088f },   // pearl
    { 0.1745f, 0.01175f, 0.01175f, 0.61424f, 0.04136f, 0.04136f, 0.727811f, 0.626959f, 0.626959f, 0.6f },   // ruby
    { 0.1f, 0.18725f, 0.1745f, 0.396f, 0.74151f, 0.69102f, 0.297254f, 0.30829f, 0.306678f, 0.1f },   // turquoise
    { 0.329412f, 0.223529f, 0.027451f, 0.780392f, 0.568627f, 0.113725f, 0.992157f, 0.941176f, 0.807843f, 0.21794872f },   // brass
    { 0.2125f, 0.1275f, 0.054f, 0.714f, 0.4284f, 0.18144f, 0.393548f, 0.271906f, 0.166721f, 0.2f },   // bronze
    { 0.25f, 0.25f, 0.25f, 0.4f, 0.4f, 0.4f, 0.774597f, 0.774597f, 0.774597f, 0.6f },   // chrome
    { 0.19125f, 0.0735f, 0.0225f, 0.7038f, 0.27048f, 0.0828f, 0.256777f, 0.137622f, 0.086014f, 0.1f },   // copper
    { 0.24725f, 0.1995f, 0.0745f, 0.75164f, 0.60648f, 0.22648f, 0.628281f, 0.555802f, 0.366065f, 0.4f },   // gold
    { 0.19225f, 0.19225f, 0.19225f, 0.50754f, 0.50754f, 0.50754f, 0.508273f, 0.508273f, 0.508273f, 0.4f },   // silver
    { 0.0f, 0.0f, 0.0f, 0.01f, 0.01f, 0.01f, 0.5f, 0.5f, 0.5f, 0.25f },   // black plastic
    { 0.0f, 0.1f, 0.06f, 0.0f, 0.5098039f, 0.5098039f, 0.50196075f, 0.50196075f, 0.50196075f, 0.25f },   // cyan plastic
    { 0.0f, 0.0f, 0.0f, 0.1f, 0.35f, 0.1f, 0.45f, 0.55f, 0.45f, 0.25f },   // green plastic
    { 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.7f, 0.6f, 0.6f, 0.25f },   // red plastic
    { 0.0f, 0.0f, 0.0f, 0.55f, 0.55f, 0.55f, 0.7f, 0.7f, 0.7f, 0.25f },   // white plastic
    { 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f, 0.6f, 0.6f, 0.5f, 0.25f },   // yellow plastic
    { 0.02f, 0.02f, 0.02f, 0.01f, 0.01f, 0.01f, 0.4f, 0.4f, 0.4f, 0.078125f },   // black rubber
    { 0.0f, 0.05f, 0.05f, 0.4f, 0.5f, 0.5f, 0.04f, 0.7f, 0.7f, 0.078125f },   // cyan rubber
    { 0.0f, 0.05f, 0.0f, 0.4f, 0.5f, 0.4f, 0.04f, 0.7f, 0.04f, 0.078125f },   // green rubber
    { 0.05f, 0.0f, 0.0f, 0.5f, 0.4f, 0.4f, 0.7f, 0.04f, 0.04f, 0.078125f },   // red rubber
    { 0.05f, 0.05f, 0.05f, 0.5f, 0.5f, 0.5f, 0.7f, 0.7f, 0.7f, 0.078125f },   // white rubber
    { 0.05f, 0.05f, 0.0f, 0.5f, 0.5f, 0.4f, 0.7f, 0.7f, 0.04f, 0.078125f },   // yellow rubber
};

// Ambient rgb, diffuse rgb, specular rgb, shininess
static FLOAT texMaterialData[NUM_TEX_MATERIALS][10] =
{
    { 0.2f, 0.2f, 0.2f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f },
    { 0.2f, 0.2f, 0.2f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.5f },
    { 0.3f, 0.2f, 0.2f, 1.0f, 0.9f, 0.8f, 1.0f, 0.9f, 0.8f, 0.5f },
    { 0.2f, 0.2f, 0.3f, 0.8f, 0.9f, 1.0f, 0.8f, 0.9f, 1.0f, 0.5f },
};

D3DMATERIAL8 g_Materials[NUM_TEA_MATERIALS + NUM_TEX_MATERIALS];




//-----------------------------------------------------------------------------
// Name: InitMaterials()
// Desc: Fills g_Materials from the tables
//-----------------------------------------------------------------------------
void InitMaterials()
{
    int i;
    FLOAT* pf;
    D3DMATERIAL8* pMat;

    for( i = 0; i < NUM_TEA_MATERIALS; i++ )
    {
        pf = teaMaterialData[i];
        pMat = &g_Materials[i];
        pMat->Ambient.r = pf[0];
        pMat->Ambient.g = pf[1];
        pMat->Ambient.b = pf[2];
        pMat->Ambient.a = 1.0f;
        pMat->Diffuse.r = pf[3];
        pMat->Diffuse.g = pf[4];
        pMat->Diffuse.b = pf[5];
        pMat->Diffuse.a = 1.0f;
        pMat->Specular.r = pf[6];
        pMat->Specular.g = pf[7];
        pMat->Specular.b = pf[8];
        pMat->Specular.a = 1.0f;
        pMat->Power = pf[9] * 128.0f;
    }

    for( i = 0; i < NUM_TEX_MATERIALS; i++ )
    {
        pf = texMaterialData[i];
        pMat = &g_Materials[NUM_TEA_MATERIALS + i];
        pMat->Ambient.r = pf[0];
        pMat->Ambient.g = pf[1];
        pMat->Ambient.b = pf[2];
        pMat->Ambient.a = 1.0f;
        pMat->Diffuse.r = pf[3];
        pMat->Diffuse.g = pf[4];
        pMat->Diffuse.b = pf[5];
        pMat->Diffuse.a = 1.0f;
        pMat->Specular.r = pf[6];
        pMat->Specular.g = pf[7];
        pMat->Specular.b = pf[8];
        pMat->Specular.a = 1.0f;
        pMat->Power = pf[9];
    }
}




//-----------------------------------------------------------------------------
// Name: RandomTeaMaterial()
// Desc: One of the good materials, for solid pipes
//-----------------------------------------------------------------------------
D3DMATERIAL8* RandomTeaMaterial()
{
    return &g_Materials[goodMaterials[CPipesScreensaver::iRand( NUM_GOOD_MATERIALS )]];
}




//-----------------------------------------------------------------------------
// Name: RandomTexMaterial()
// Desc: One of the materials for textured pipes
//-----------------------------------------------------------------------------
D3DMATERIAL8* RandomTexMaterial()
{
    return &g_Materials[NUM_TEA_MATERIALS + CPipesScreensaver::iRand( NUM_TEX_MATERIALS )];
}
