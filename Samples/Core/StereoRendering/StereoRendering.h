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
#pragma once
#include "Falcor.h"

using namespace Falcor;

class StereoRendering : public Sample
{
public:
    void onLoad() override;
    void onFrameRender() override;
    void onResizeSwapChain() override;
    bool onKeyEvent(const KeyboardEvent& keyEvent) override;
    bool onMouseEvent(const MouseEvent& mouseEvent) override;
    void onDataReload() override;

private:
    void initUI();

    Scene::SharedPtr mpScene;
    SceneRenderer::UniquePtr mpSceneRenderer;

    Program::SharedPtr mpProgram = nullptr;
    UniformBuffer::SharedPtr mpUniformBuffer = nullptr;

    void loadScene();
    static void GUI_CALL loadSceneCB(void* pThis);

    SceneRenderer::RenderMode mRenderMode = SceneRenderer::RenderMode::Mono;
    void submitToScreen();
    void initVR();
    void blitTexture(const Texture* pTexture, uint32_t xStart);
    VrFbo::UniquePtr mpVrFbo;
    FullScreenPass::UniquePtr mpBlit;
    UniformBuffer::SharedPtr mpBlitUbo;
    bool mShowStereoViews = true;
    void submitSinglePassStereo();
    void setRenderMode();

    static void GUI_CALL getRenderModeCB(void* pVal, void* pUserData);
    static void GUI_CALL setRenderModeCB(const void* pVal, void* pUserData);
};
