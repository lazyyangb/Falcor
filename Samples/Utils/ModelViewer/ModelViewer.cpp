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
#include "ModelViewer.h"

void GUI_CALL ModelViewer::loadModelCallback(void* pUserData)
{
    ModelViewer* pViewer = reinterpret_cast<ModelViewer*>(pUserData);
    pViewer->loadModel();
}

void GUI_CALL ModelViewer::saveModelCallback(void* pUserData)
{
    ModelViewer* pViewer = reinterpret_cast<ModelViewer*>(pUserData);
    pViewer->saveModel();
}

void GUI_CALL ModelViewer::deleteCulledMeshesCallback(void* pUserData)
{
    ModelViewer* pViewer = reinterpret_cast<ModelViewer*>(pUserData);
    if(pViewer->mpModel)
    {
        CpuTimer timer;
        timer.update();
        pViewer->mpModel->deleteCulledMeshes(pViewer->mpCamera.get());
        timer.update();

        pViewer->setModelString(true, timer.getElapsedTime());
    }
}

CameraController& ModelViewer::getActiveCameraController()
{
    switch(mCameraType)
    {
    case ModelViewer::ModelViewCamera:
        return mModelViewCameraController;
    case ModelViewer::FirstPersonCamera:
        return mFirstPersonCameraController;
    case ModelViewer::SixDoFCamera:
        return m6DoFCameraController;
    default:
        should_not_get_here();
        return m6DoFCameraController;
    }
}

void ModelViewer::setModelString(bool isAfterCull, float loadTime)
{
    mModelString = isAfterCull ? "Mesh culling" : "Loading";
    mModelString += " took " + std::to_string(loadTime) + " seconds.\n";
    mModelString += "Model has " + std::to_string(mpModel->getVertexCount()) + " vertices, ";
    mModelString += std::to_string(mpModel->getPrimitiveCount()) + " primitives, ";
    mModelString += std::to_string(mpModel->getMeshCount()) + " meshes, ";
    mModelString += std::to_string(mpModel->getInstanceCount()) + " mesh instances, ";
    mModelString += std::to_string(mpModel->getMaterialCount()) + " materials, ";
    mModelString += std::to_string(mpModel->getTextureCount()) + " textures, ";
    mModelString += std::to_string(mpModel->getBufferCount()) + " buffers.\n";
}

void ModelViewer::loadModelFromFile(const std::string& filename)
{
    CpuTimer timer;
    timer.update();

    uint32_t flags = mCompressTextures ? Model::CompressTextures : 0;
    flags |= mGenerateTangentSpace ? Model::GenerateTangentSpace : 0;
    auto fboFormat = mpDefaultFBO->getColorTexture(0)->getFormat();
    flags |= isSrgbFormat(fboFormat) ? 0 : Model::AssumeLinearSpaceTextures;
    mpModel = Model::createFromFile(filename, flags);

    if(mpModel == nullptr)
    {
        msgBox("Could not load model");
        return;
    }
    setModelUIElements();
    resetCamera();

    float radius = mpModel->getRadius();
    float lightHeight = Falcor::max(1.0f + radius, radius*1.25f);
    mpPointLight->setWorldPosition(glm::vec3(0, lightHeight, 0));
    timer.update();

    setModelString(false, timer.getElapsedTime());
}

void ModelViewer::loadModel()
{
    std::string Filename;
    if(openFileDialog(Model::kSupportedFileFormatsStr, Filename))
    {
        loadModelFromFile(Filename);
    }
}

void ModelViewer::saveModel()
{
    if(mpModel == nullptr)
    {
        msgBox("No model was loaded. Nothing to save");
        return;

    }
    std::string filename;
    if(saveFileDialog("Binary Model\0*.bin\0\0", filename))
    {
        mpModel->exportToBinaryFile(filename);
    }
}

void ModelViewer::initUI()
{
    Gui::setGlobalHelpMessage("Sample application to load and display a model.\nUse the UI to switch between wireframe and solid mode.");
    // Load model group
    mpGui->addButton("Load Model", &ModelViewer::loadModelCallback, this);
    const std::string LoadOptions = "Load Options";
    mpGui->addCheckBox("Compress Textures", &mCompressTextures, LoadOptions);
    mpGui->addCheckBox("Generate Tangent Space", &mGenerateTangentSpace, LoadOptions);
    mpGui->addButton("Export Model To Binary File", &ModelViewer::saveModelCallback, this);
    mpGui->addButton("Delete Culled Meshes", &ModelViewer::deleteCulledMeshesCallback, this);

    mpGui->addSeparator();
    mpGui->addCheckBox("Wireframe", &mDrawWireframe);
    mpGui->addCheckBox("TriLinear Filtering", &mUseTriLinearFiltering);

    Gui::dropdown_list cullList;
    cullList.push_back({0, "No Culling"});
    cullList.push_back({1, "Backface Culling"});
    cullList.push_back({2, "Frontface Culling"});
    mpGui->addDropdown("Cull Mode", cullList, &mCullMode);

    const std::string lightGroup = "Lights";
    mpGui->addRgbColor("Ambient intensity", &mAmbientIntensity, lightGroup);
    mpDirLight->setUiElements(mpGui.get(), "Directional Light");
    mpGui->nestGroups(lightGroup, "Directional Light");
    mpPointLight->setUiElements(mpGui.get(), "Point Light");
    mpGui->nestGroups(lightGroup, "Point Light");

    Gui::dropdown_list cameraDropdown;
    cameraDropdown.push_back({ModelViewCamera, "Model-View"});
    cameraDropdown.push_back({FirstPersonCamera, "First-Person"});
    cameraDropdown.push_back({SixDoFCamera, "6 DoF"});

    mpGui->addDropdown("Camera Type", cameraDropdown, &mCameraType);


    uint32_t barSize[2];
    mpGui->getSize(barSize);
    barSize[0] += 50;
    barSize[1] += 100;
    mpGui->setSize(barSize[0], barSize[1]);
}

void ModelViewer::setModelUIElements()
{
    bool bAnim = mpModel && mpModel->hasAnimations();
    static const char* animateStr = "Animate";
    static const char* activeAnimStr = "Active Animation";

    if(bAnim)
    {
        mActiveAnimationID = sBindPoseAnimationID;

        mpGui->addCheckBox(animateStr, &mAnimate);
        Gui::dropdown_list list;
        list.resize(mpModel->getAnimationsCount() + 1);
        list[0].label = "Bind Pose";
        list[0].value = sBindPoseAnimationID;

        for(uint32_t i = 0; i < mpModel->getAnimationsCount(); i++)
        {
            list[i + 1].value = i;
            list[i + 1].label = mpModel->getAnimationName(i);
            if(list[i + 1].label.size() == 0)
            {
                list[i + 1].label = std::to_string(i);
            }
        }
        mpGui->addDropdownWithCallback(activeAnimStr, list, SetActiveAnimationCB, GetActiveAnimationCB, this);
    }
    else
    {
        mpGui->removeVar(animateStr);
        mpGui->removeVar(activeAnimStr);
    }

    mpGui->removeVar("Near Plane", "Depth Range");
    mpGui->removeVar("Far Plane", "Depth Range");
    const float minDepth = mpModel->getRadius() * 1 / 1000;
    mpGui->addFloatVar("Near Plane", &mNearZ, "Depth Range", minDepth, mpModel->getRadius() * 15, minDepth * 5);
    mpGui->addFloatVar("Far Plane", &mFarZ, "Depth Range", minDepth, mpModel->getRadius() * 15, minDepth * 5);
}

void ModelViewer::onLoad()
{
    mpCamera = Camera::create();
    mpProgram = Program::createFromFile("", "ModelViewer.fs");

    // create rasterizer state
    RasterizerState::Desc wireframeDesc;
    wireframeDesc.setFillMode(RasterizerState::FillMode::Wireframe);
    wireframeDesc.setCullMode(RasterizerState::CullMode::None);
    mpWireframeRS = RasterizerState::create(wireframeDesc);

    RasterizerState::Desc solidDesc;
    solidDesc.setCullMode(RasterizerState::CullMode::None);
    mpCullRastState[0] = RasterizerState::create(solidDesc);
    solidDesc.setCullMode(RasterizerState::CullMode::Back);
    mpCullRastState[1] = RasterizerState::create(solidDesc);
    solidDesc.setCullMode(RasterizerState::CullMode::Front);
    mpCullRastState[2] = RasterizerState::create(solidDesc);

    // Depth test
    DepthStencilState::Desc dsDesc;
	dsDesc.setDepthTest(false);
	mpNoDepthDS = DepthStencilState::create(dsDesc);
    dsDesc.setDepthTest(true);
    mpDepthTestDS = DepthStencilState::create(dsDesc);

    mModelViewCameraController.attachCamera(mpCamera);
    mFirstPersonCameraController.attachCamera(mpCamera);
    m6DoFCameraController.attachCamera(mpCamera);

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    mpPointSampler = Sampler::create(samplerDesc);
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    mpLinearSampler = Sampler::create(samplerDesc);

    mpDirLight = DirectionalLight::create();
    mpPointLight = PointLight::create();
    mpDirLight->setWorldDirection(glm::vec3(0.13f, 0.27f, -0.9f));

    mpPerFrameCB = UniformBuffer::create(mpProgram->getActiveProgramVersion().get(), "PerFrameCB");

    initUI();
}

void ModelViewer::onFrameRender()
{
    const glm::vec4 clearColor(0.38f, 0.52f, 0.10f, 1);
    mpDefaultFBO->clear(clearColor, 1.0f, 0, FboAttachmentType::All);

    if(mpModel)
    {
        mpCamera->setDepthRange(mNearZ, mFarZ);
        getActiveCameraController().update();

        // Animate
        if(mAnimate)
        {
            PROFILE(animate);
            mpModel->animate(mCurrentTime);
        }

        // Set render state
        if(mDrawWireframe)
        {
            mpRenderContext->setRasterizerState(mpWireframeRS);
            mpRenderContext->setDepthStencilState(mpNoDepthDS, 0);
            mpPerFrameCB->setVariable("gConstColor", true);
        }
        else
        {
            mpRenderContext->setRasterizerState(mpCullRastState[mCullMode]);
            mpRenderContext->setDepthStencilState(mpDepthTestDS, 0);
            mpPerFrameCB->setVariable("gConstColor", false);
            mpDirLight->setIntoUniformBuffer(mpPerFrameCB.get(), "gDirLight");
            mpPointLight->setIntoUniformBuffer(mpPerFrameCB.get(), "gPointLight");
        }

        if(mUseTriLinearFiltering)
		{
			mpModel->bindSamplerToMaterials(mpLinearSampler);
        }
        else
        {
            mpModel->bindSamplerToMaterials(mpPointSampler);
        }

        mpPerFrameCB->setVariable("gAmbient", mAmbientIntensity);
        mpRenderContext->setUniformBuffer(0, mpPerFrameCB);
        ModelRenderer::render(mpRenderContext.get(), mpProgram.get(), mpModel, mpCamera.get());
    }

    std::string Msg = getGlobalSampleMessage(true) + '\n' + mModelString;
    renderText(Msg, glm::vec2(10, 10));
}

void ModelViewer::onShutdown()
{

}

bool ModelViewer::onKeyEvent(const KeyboardEvent& keyEvent)
{
    bool bHandled = getActiveCameraController().onKeyEvent(keyEvent);
    if(bHandled == false)
    {
        if(keyEvent.type == KeyboardEvent::Type::KeyPressed)
        {
            switch(keyEvent.key)
            {
            case KeyboardEvent::Key::R:
                resetCamera();
                bHandled = true;
                break;
            }
        }
    }
    return bHandled;
}

bool ModelViewer::onMouseEvent(const MouseEvent& mouseEvent)
{
    return getActiveCameraController().onMouseEvent(mouseEvent);
}

void ModelViewer::onResizeSwapChain()
{
    RenderContext::Viewport vp;
    vp.height = (float)mpDefaultFBO->getHeight();
    vp.width = (float)mpDefaultFBO->getWidth();
    mpRenderContext->setViewport(0, vp);

    mpCamera->setFovY(float(M_PI / 3));
    float aspectRatio = (vp.width / vp.height);
    mpCamera->setAspectRatio(aspectRatio);
}

void ModelViewer::resetCamera()
{
    if(mpModel)
    {
        // update the camera position
        float Radius = mpModel->getRadius();
        const glm::vec3& ModelCenter = mpModel->getCenter();
        glm::vec3 CamPos = ModelCenter;
        CamPos.z += Radius * 5;

        mpCamera->setPosition(CamPos);
        mpCamera->setTarget(ModelCenter);
        mpCamera->setUpVector(glm::vec3(0, 1, 0));

        // Update the controllers
        mModelViewCameraController.setModelParams(ModelCenter, Radius, 3.5f);
        mFirstPersonCameraController.setCameraSpeed(Radius*0.25f);
        m6DoFCameraController.setCameraSpeed(Radius*0.25f);

        mNearZ = std::max(0.1f, mpModel->getRadius() / 750.0f);
        mFarZ = Radius * 10;
    }
}

void ModelViewer::GetActiveAnimationCB(void* pVal, void* pUserData)
{
    ModelViewer* pViewer = (ModelViewer*)pUserData;
    *(uint32_t*)pVal = pViewer->mActiveAnimationID;
}

void ModelViewer::SetActiveAnimationCB(const void* pVal, void* pUserData)
{
    ModelViewer* pViewer = (ModelViewer*)pUserData;
    pViewer->mActiveAnimationID = *(uint32_t*)pVal;
    pViewer->mpModel->setActiveAnimation(pViewer->mActiveAnimationID);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    ModelViewer modelViewer;
    SampleConfig config;
    config.windowDesc.title = "Falcor Model Viewer";
    modelViewer.run(config);
}
