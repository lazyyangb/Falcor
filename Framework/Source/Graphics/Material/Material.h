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
#include "glm/vec3.hpp"
#include <map>
#include <vector>
#include "glm/common.hpp"
#include "glm/geometric.hpp"
#include "Core/Texture.h"
#include "glm/mat4x4.hpp"
#include "Data/HostDeviceData.h"
#include "Core/Sampler.h"

namespace Falcor
{
    class Texture;
    class UniformBuffer;

    /** A surface material object
        The core part of material is the 'SMaterial	m_Material' data structure. It consists of multiple layers and modifiers.
        Modifiers can modify shading attributes *before* performing the actual shading. So far only two modifiers are supported:
            - SMaterial::AlphaMap               - Alpha test. 
                                                    Decides whether to shade at all. 
                                                    It invalidates the shading hit based on the threshold of opacity coming from the alpha texture.
                                                    'ConstantColor.x' is used as alpha threshold for alpha testing (from 0 to 1).
            - SMaterial::NormalMap              - Normal mapping. Rotates the shading frame before performing shading.
                                                    'ConstantColor' is not used, only the texture slot.
            - [Reserved] SMaterial::HeightMap   - Moves the shading hit point along the shading normal by the amount fetched from height map texture. 
                                                    Can be used in parallax and relief mapping in pixel shader, or as a displacement map during tessellation stage.
                                                    'ConstantColor' contains bias (level offset) of the height map in 'x' component and scale of the height map in 'y' component. 
                                                    Height is meant to be computed as h = ConstantColor.x + fetch(HeightMap).x * ConstantColor.y
        After modifiers are applied, the shading is performed with 'MatMaxLayers' number of layers (MatMaxLayers can be adjusted, by default is 3). Each layer can be one of the following types (SMaterialLayer::Type variable):
            - kMatLambert                        - The simplest Lambertian diffuse BRDF. 
                                                    'Albedo' is used as albedo color. 
                                                        .ConstantColor.rgb      - constant base color.
                                                        .Texture.rgb            - spatially-varying albedo modulator. 
                                                    Other SMaterialValue parameters are not used for this Type of layer.
            - kMatConductor                        - The conductor BRDF. Has metallic glossy look, designed for metals and compute corresponding conductor Fresnel.
                                                    'NDF'                   - Normal Distribution Function (NDF) Type, which is responsible for rough look of the material and takes roughness as an input. Can take on the following values
                                                                                'kNDFGGX'        - (default value) modern distribution model, characterized by a more smooth highlight
                                                                                'kNDFBeckmann'   - Blinn-Phong normal distribution, defined as 2D Gaussian distribution of microfacets' slopes, has sharp highlight.
                                                    'Albedo' is used as specular reflection color.
                                                    .ConstantColor.rgb      - constant base color.
                                                    .Texture.rgb            - spatially-varying modulator.
                                                    'Roughness' is used as material roughness, takes values from 0 (perfect smooth specular) to 1 (highest roughness, diffuse-alike).
                                                    .ConstantColor.x      - constant base roughness.
                                                    .Texture.x            - spatially-varying modulator.
                                                    'ExtraParam.ConstantColor' is used to store Fresnel parameters of the conductor.
                                                    .x              - real part of complex index of refraction for conductors (actual values are a common knowledge for many metals).
                                                    .y              - imaginary part of complex index of refraction for conductors.
                                                    Other SMaterialValue parameters are not used for this Type of layer.
            - kMatDielectric                       - The dielectric BSDF. Has plastic specular/glossy transparent look, designed for transparent dielectrics (like glass, water, coatings etc.) and compute corresponding dielectric Fresnel.
                                                    'Albedo' is used as transmission/reflection color. 
                                                    .ConstantColor.rgb      - constant base color.
                                                    .Texture.rgb            - spatially-varying modulator.
                                                    'Roughness' is used as material roughness, takes values from 0 (perfect smooth specular) to 1 (highest roughness, diffuse-alike).
                                                    .ConstantColor.x      - constant base roughness.
                                                    .Texture.x            - spatially-varying modulator.
                                                    'ExtraParam.ConstantColor.x' is used to store index of refraction for dielectric Fresnel parameter (common knowledge, e.g. ~1.3 for unsalted water).
                                                    Other SMaterialValue parameters are not used for this Type of layer.
        Besides that, each layer defines how it will compose its shading results with the results of previous layers. 
        The shading happens in a last-to-first order, so each next layer with smaller index composits its shading results with the previous results according to the value of 'Blending' variable of this layer:
            - kBlendFresnel                        - Composite the results of this layer according to the Fresnel value of the layer. Available only for layers with Fresnel (conductor/dielectric).
                                                    Useful for describing a material with e.g. a dielectric coating, where the underlying material layer is blended according to the dielectric Fresnel-based transmission.
            - BlendConstant                       - Constant-weight blending between this layer and the previous result. The constant value is taken from ConstantColor.a component of the current layer.
                                                    Useful for mixing multiple BSDFs, e.g. diffuse+conductor, or a multi-lobe conductor.
            - BlendAdd                            - Just add the shading results of this layer to the results of previous shading.
    */
    class Material : public std::enable_shared_from_this<Material>
    {
    public:
        using SharedPtr = std::shared_ptr<Material>;
        using SharedConstPtr = std::shared_ptr<const Material>;

		/** create a new material
            \param[in] name the material name
        */
        static SharedPtr create(const std::string& name);

        ~Material();

        /** Set the material name
        */
        void setName(const std::string& name) { mName = name; }

        /** Get the material name
        */
        const std::string& getName() const { return mName; }

        /** Get the material ID. 
        */
        const int32_t getId() const { return mData.values.id; }

        /** Set the material ID
        */
        void setID(int32_t id) { mData.values.id = id; }
        
        /** Reset all global id counter of model, mesh and material
        */
        static void resetGlobalIdCounter();

		/** API for working with layers */

		/** Returns the number of active layers in the material
		*/
		size_t getNumActiveLayers() const;

		/** Returns a material layer descriptor, or null if there is no layer with this index
            \param[in] layerIdx The index of the material layer
		*/
		const MaterialLayerDesc* getLayerDesc(uint32_t layerIdx) const;

        /** Returns a material layer values, or null if there is no layer with this index
        \param[in] layerIdx The index of the material layer
        */
        const MaterialLayerValues* getLayerValues(uint32_t layerIdx) const;

		/** Adds a layer to the material. Returns true if succeeded
			\param[in] layer The material layer to add
		*/
		bool addLayer(const MaterialLayerDesc& desc, const MaterialLayerValues& values);

		/** Removes a layer of the material.
            \param[in] layerIdx The index of the material layer
		*/
		void removeLayer(uint32_t layerIdx);
        
        /** Set the normal map value
        */
        void setNormalValue(const MaterialValue& normal);
    
		/** Returns the normal map value
		*/
		const MaterialValue& getNormalValue() const { return mData.values.normalMap; }

        /** Set the alpha test value
        */
        void setAlphaValue(const MaterialValue& alpha);

        /** Set the ambient occlusion value
        */
        void setAmbientValue(const MaterialValue& ambient);

        /** Returns the alpha test value
        */
        const MaterialValue& getAlphaValue() const { return mData.values.alphaMap; }

        /** Returns the ambient value
        */
        const MaterialValue& getAmbientValue() const { return mData.values.ambientMap; }

        /** Set the height map value
        */
        void setHeightValue(const MaterialValue& height);

        /** Returns the height map value
        */
        const MaterialValue& getHeightValue() const { return mData.values.heightMap; }

		/** Helper method, returns all active textures
            \param[out] Textures The list of active textures
		*/
        void getActiveTextures(std::vector<Texture::SharedConstPtr>& textures) const;

		/** Check if this is a double-sided material. Meshes with double sided materials should be drawn without culling, and for backfacing polygons, the normal has to be inverted.
        */
        bool isDoubleSided() const      { return mDoubleSided; }
        /** Set the material as double-sided. Meshes with double sided materials should be drawn without culling, and for backfacing polygons, the normal has to be inverted.
        */
        void setDoubleSided(bool doubleSided) { mDoubleSided = doubleSided; mDescDirty = true; }

        /** Set the material parameters into a uniform buffer. To use this you need to include 'Falcor.h' inside your shader.
            \param[in] pBuffer The uniform buffer to set the parameters into.
            \param[in] VarName The name of the material variable in the program.
        */
        void setIntoUniformBuffer(UniformBuffer* pBuffer, const std::string& varName) const;
        
        /** Returns the raw material data
        */
        const MaterialData& getData() const     { return mData; }
        
        /** Override all sampling types of materials
        */
        void overrideAllSamplers(const Sampler::SharedPtr& pSampler) { mpSamplerOverride = pSampler; }
                
        /** Return global sampler override 
        */
        const Sampler::SharedPtr& getSamplerOverride() const { return mpSamplerOverride; }

        /** Binds all the textures to the GPU
        */
        void bindTextures() const;

        /** Unload all the textures from the GPU memory.
        */
        void unloadTextures() const;

        /** Comparison operator
        */
        bool operator==(const Material& other) const;

        /** The a string for a MaterialDesc string which can be patched into the shader. It can be used to statically compile the material into a program, resulting in better generated code
        */
        void getMaterialDescStr(std::string& shaderDcl) const;

        /** Get an identifier for material desc. This can be used for fast comparison of desc objects. 2 materials will have the same identifier if and only if their desc is exactly the same.
        */
        uint64_t getDescIdentifier() const;

        /** Validates the layers of the material, making sure that it is energy conserving.
        */
        void finalize() const;
    private:
        mutable bool mDescDirty   = false;
        mutable size_t mDescIdentifier;
        void updateDescIdentifier() const;
        void removeDescIdentifier() const;

        void normalize();

		static uint32_t sMaterialCounter;
        struct DescId
        {
            MaterialDesc desc;
            uint64_t id;
            uint32_t refCount;
        };
        // Can't use map directly, since it requires a 'less' operator for the MaterialDesc.
        // vector is slower, but materials are usually not dirty, so shouldn't really affect performance
        static std::vector<DescId> sDescIdentifier;

		/** create a new material
            \param[in] Name The material name
        */
		Material(const std::string& name);
		
        mutable MaterialData mData;				///< Material data shared between host and device

		bool				mDoubleSided = false;	///< Used for culling 

		Sampler::SharedPtr mpSamplerOverride = nullptr;

        std::string			mName;
    };
}