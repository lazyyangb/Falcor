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
#include "VrFbo.h"
#include "OpenVR/VRSystem.h"
#include "OpenVR/VRDisplay.h"
#include "Graphics/FboHelper.h"
#include "Core/Texture.h"

namespace Falcor
{
    glm::ivec2 getHmdRenderSize()
    {
        VRSystem* pVrSystem = VRSystem::instance();
        VRDisplay* pDisplay = pVrSystem->getHMD().get();
        glm::ivec2 renderSize = pDisplay->getRecommendedRenderSize();
        return renderSize;
    }

    VrFbo::UniquePtr VrFbo::create(ResourceFormat colorFormat[], uint32_t rtCount, ResourceFormat depthFormat, uint32_t sampleCount, const glm::ivec2& rtSize)
    {
        glm::ivec2 renderSize = (rtSize.x == 0 && rtSize.y == 0) ? getHmdRenderSize() : rtSize;

        // Create the FBO
        VrFbo::UniquePtr pVrFbo = std::make_unique<VrFbo>();
        pVrFbo->mpFbo = FboHelper::create2DWithDepth(renderSize.x, renderSize.y, colorFormat, depthFormat, 2, rtCount, sampleCount);

        // Get the left and right eye views
        pVrFbo->mpLeftView.resize(rtCount);
        pVrFbo->mpRightView.resize(rtCount);
        for(uint32_t rt = 0 ; rt < rtCount ; rt++)
        {
            pVrFbo->mpLeftView[rt] = pVrFbo->mpFbo->getColorTexture(rt)->createView(0, 1, 0, 1);
            pVrFbo->mpRightView[rt] = pVrFbo->mpFbo->getColorTexture(rt)->createView(1, 1, 0, 1);
        }

        return pVrFbo;
    }

    void VrFbo::pushViewport(RenderContext* pContext, uint32_t vpIndex)
    {
        RenderContext::Viewport vp = pContext->getViewport(vpIndex);
        vp.originX = 0;
        vp.originY = 0;
        vp.width = (float)mpFbo->getWidth();
        vp.height = (float)mpFbo->getHeight();
        pContext->pushViewport(vpIndex, vp);
    }

    void VrFbo::submitToHmd(uint32_t rtIndex) const
    {
        VRSystem* pVrSystem = VRSystem::instance();
        pVrSystem->submit(VRDisplay::Eye::Left, mpLeftView[rtIndex]);
        pVrSystem->submit(VRDisplay::Eye::Right, mpRightView[rtIndex]);
    }
}