/***************************************************************************
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
#include "Framework.h"
#include "Font.h"
#include "TextRenderer.h"
#include <vector>
#include "glm/mat4x4.hpp"

namespace Falcor
{
    static const glm::vec2 kVertexPos[] =
    {
        glm::vec2(0, 0),
        glm::vec2(0, 1),
        glm::vec2(1, 0),

        glm::vec2(1, 0),
        glm::vec2(0, 1),
        glm::vec2(1, 1),
    };

    TextRenderer::UniquePtr TextRenderer::create()
    {
        return UniquePtr(new TextRenderer());
    }

    TextRenderer::TextRenderer()
    {
        static const std::string kVsFile("Framework\\TextRenderer.vs");
        static const std::string kFsFile("Framework\\TextRenderer.fs");

        mpProgram = Program::createFromFile(kVsFile, kFsFile);
        const uint32_t vbSize = (uint32_t)(sizeof(Vertex)*kMaxBatchSize*arraysize(kVertexPos));
        mpVertexBuffer = Buffer::create(vbSize, Buffer::BindFlags::Vertex, Buffer::AccessFlags::MapWrite, nullptr);
        
        createVAO();
        // create the depth-state
        DepthStencilState::Desc dsDesc;
        dsDesc.setDepthTest(false).setStencilTest(false);
        mpDepthStencilState = DepthStencilState::create(dsDesc);
        
        // Rasterizer state
        RasterizerState::Desc rsState;
        rsState.setCullMode(RasterizerState::CullMode::None);
        mpRasterizerState = RasterizerState::create(rsState);

        // Blend state
        BlendState::Desc blendDesc;
        blendDesc.setRtBlend(0, true).setRtParams(0, BlendState::BlendOp::Add,
            BlendState::BlendOp::Add,
            BlendState::BlendFunc::SrcAlpha,
            BlendState::BlendFunc::OneMinusSrcAlpha,
            BlendState::BlendFunc::One,
            BlendState::BlendFunc::One);

        mpBlendState = BlendState::create(blendDesc);
        mpFont = Font::create();

        // Uniform buffer
        mpPerFrameCB = UniformBuffer::create(mpProgram->getActiveProgramVersion().get(), "PerFrameCB");
        mUniformOffsets.vpTransform = mpPerFrameCB->getVariableOffset("gvpTransform");
        mUniformOffsets.fontColor = mpPerFrameCB->getVariableOffset("gFontColor");
        mUniformOffsets.fontTex = mpPerFrameCB->getVariableOffset("gFontTex");
    }

    TextRenderer::~TextRenderer() = default;

    void TextRenderer::createVAO()
    {
        Vao::VertexBufferDescVector VbDesc(1);
        VbDesc[0].pLayout->addElement("POSITION", 0, ResourceFormat::RG32Float, 1, 0);
        VbDesc[0].pLayout->addElement("TEXCOORD", 8, ResourceFormat::RG32Float, 1, 1);
        VbDesc[0].stride = sizeof(Vertex);
        VbDesc[0].pBuffer = mpVertexBuffer;

        mpVAO = Vao::create(VbDesc, nullptr);
    }

    void TextRenderer::begin(const RenderContext::SharedPtr& pRenderContext, const glm::vec2& startPos)
    {
        mCurPos = startPos;
        mStartPos = startPos;
        mpRenderContext = pRenderContext;

        // Save state
        pRenderContext->pushState();

        // Set shaders
        pRenderContext->setProgram(mpProgram->getActiveProgramVersion());
        pRenderContext->setVao(mpVAO);

        // Get the current viewport
        const auto& VP = pRenderContext->getViewport(0);

        // Set the matrix
        glm::mat4 vpTransform;
        vpTransform[0][0] = 2 / VP.width;
        vpTransform[1][1] = -2 / VP.height;
        vpTransform[3][0] = -(VP.originX + VP.width) / VP.width;
        vpTransform[3][1] = (VP.originX + VP.height) / VP.height;

        mpPerFrameCB->setVariable(mUniformOffsets.vpTransform, vpTransform);
        mpPerFrameCB->setVariable(mUniformOffsets.fontColor, mTextColor);
        
        // Set the font texture
        const Texture* pFontTex = mpFont->getTexture();
        mpPerFrameCB->setTexture(mUniformOffsets.fontTex, pFontTex, nullptr);

        pRenderContext->setUniformBuffer(0, mpPerFrameCB);

        // Set state
        pRenderContext->setRasterizerState(mpRasterizerState);
        pRenderContext->setDepthStencilState(mpDepthStencilState, 0);
        pRenderContext->setBlendState(mpBlendState);
        pRenderContext->setTopology(RenderContext::Topology::TriangleList);

        // Map the buffer
        mpBufferData = (Vertex*)mpVertexBuffer->map(Buffer::MapType::WriteDiscard);
    }

    void TextRenderer::end()
    {
        flush();
        mpRenderContext->popState();
        mpRenderContext = nullptr;
        mpVertexBuffer->unmap();
    }

    void TextRenderer::flush()
    {
        if(mCurrentVertexID != 0)
        {
            mpVertexBuffer->unmap();
            mpRenderContext->draw(mCurrentVertexID, 0);
            mCurrentVertexID = 0;
            mpVertexBuffer->map(Buffer::MapType::WriteDiscard);
        }
    }

    void TextRenderer::renderLine(const std::string& line)
    {
        for(size_t CurChar = 0; CurChar < line.size() ; CurChar++)
        {
            // Make sure we enough space for the next char
            if(mCurrentVertexID + arraysize(kVertexPos) > mpVertexBuffer->getSize())
            {
                flush();
            }

            char c = line[CurChar];
            if(c == '\n')
            {
                mCurPos.y += mpFont->getFontHeight();
                mCurPos.x = mStartPos.x;
            }
            else if(c == '\t')
            {
                mCurPos.x += mpFont->getTabWidth();
            }
            else if(c == ' ')
            {
                mCurPos.x += mpFont->getLettersSpacing();
            }
            else
            {
                // Regular character
                const Font::CharTexCrdDesc& desc = mpFont->getCharDesc(c);
                for(uint32_t i = 0; i < arraysize(kVertexPos); i++, mCurrentVertexID++)
                {
                    glm::vec2 posScale = kVertexPos[i];
#ifdef FALCOR_GL
                    // Invert the y-axis for opengl
                    posScale.y = 1 - posScale.y;

#endif
                    glm::vec2 pos = desc.size * posScale;
                    pos += mCurPos;
                    mpBufferData[mCurrentVertexID].screenPos = pos;
                    mpBufferData[mCurrentVertexID].texCoord = desc.topLeft + desc.size * kVertexPos[i];
                }

                mCurPos.x += mpFont->getLettersSpacing();
            }
        }
        mCurPos.y += mpFont->getFontHeight();
        mCurPos.x = mStartPos.x;
    }
}
