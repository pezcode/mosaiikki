#pragma once

#include "Drawables/InstanceDrawable.h"
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Texture.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Utility/Assert.h>
#include <algorithm>

template<typename Transform>
class TexturedDrawable : public Magnum::SceneGraph::Drawable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    explicit TexturedDrawable(
        Object3D& object,
        Magnum::Shaders::Phong& shader,
        Magnum::UnsignedInt meshId,
        Magnum::GL::Mesh& mesh,
        Magnum::GL::Buffer& instanceBuffer,
        Corrade::Containers::ArrayView<Corrade::Containers::Pointer<Magnum::GL::Texture2D>> textures,
        const Magnum::Trade::PhongMaterialData& material) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(shader),
        _meshId(meshId),
        _mesh(mesh),
        instanceBuffer(instanceBuffer),
        material(material)
    {
        ambientTexture = diffuseTexture = specularTexture = normalTexture = nullptr;

        Magnum::Trade::PhongMaterialData::Flags requiredFlags = requiredMaterialFlags(shader);
        CORRADE_ASSERT((requiredFlags & material.flags()) == requiredFlags, "Material missing required textures", );

        if(requiredFlags & Magnum::Trade::PhongMaterialData::Flag::AmbientTexture)
            ambientTexture = textures[material.ambientTexture()].get();
        if(requiredFlags & Magnum::Trade::PhongMaterialData::Flag::DiffuseTexture)
            diffuseTexture = textures[material.diffuseTexture()].get();
        if(requiredFlags & Magnum::Trade::PhongMaterialData::Flag::SpecularTexture)
            specularTexture = textures[material.specularTexture()].get();
        if(requiredFlags & Magnum::Trade::PhongMaterialData::Flag::NormalTexture)
            normalTexture = textures[material.normalTexture()].get();
    }

    Magnum::UnsignedInt meshId() const
    {
        return _meshId;
    }

    InstanceDrawable<Transform>& addInstance(Object3D& object)
    {
        InstanceDrawable<Transform>* instance = new InstanceDrawable<Transform>(object, instanceData);
        instanceDrawables.add(*instance);
        return *instance;
    }

    Magnum::SceneGraph::DrawableGroup3D& instances()
    {
        return instanceDrawables;
    }

    static Magnum::Trade::PhongMaterialData::Flags requiredMaterialFlags(const Magnum::Shaders::Phong& shader)
    {
        Magnum::Trade::PhongMaterialData::Flags flags = {};
        if(shader.flags() & Magnum::Shaders::Phong::Flag::AmbientTexture)
            flags |= Magnum::Trade::PhongMaterialData::Flag::AmbientTexture;
        if(shader.flags() & Magnum::Shaders::Phong::Flag::DiffuseTexture)
            flags |= Magnum::Trade::PhongMaterialData::Flag::DiffuseTexture;
        if(shader.flags() & Magnum::Shaders::Phong::Flag::SpecularTexture)
            flags |= Magnum::Trade::PhongMaterialData::Flag::SpecularTexture;
        if(shader.flags() & Magnum::Shaders::Phong::Flag::NormalTexture)
            flags |= Magnum::Trade::PhongMaterialData::Flag::NormalTexture;
        return flags;
    }

private:
    virtual void draw(const Magnum::Matrix4& /*transformationMatrix*/, Magnum::SceneGraph::Camera3D& camera) override
    {
        if(instanceDrawables.isEmpty())
            return;

        /*
        // sort objects back to front for correct alpha blending
        // not needed currently since we add instanced in our scene in the correct order and the camera position is static
        std::vector<std::pair<std::reference_wrapper<SceneGraph::Drawable3D>, Matrix4>> drawableTransformations =
            camera.drawableTransformations(instanceDrawables);
        
        std::sort(drawableTransformations.begin(),
                  drawableTransformations.end(),
                  [](const std::pair<std::reference_wrapper<SceneGraph::Drawable3D>, Matrix4>& a,
                     const std::pair<std::reference_wrapper<SceneGraph::Drawable3D>, Matrix4>& b) {
                      return a.second.translation().z() < b.second.translation().z();
                  });
        */

        Corrade::Containers::arrayResize(instanceData, 0);
        camera.draw(instanceDrawables /*drawableTransformations*/);

        instanceBuffer.setData(instanceData, Magnum::GL::BufferUsage::DynamicDraw);
        _mesh.setInstanceCount(instanceData.size());

        if(ambientTexture || diffuseTexture || specularTexture || normalTexture)
            shader.bindTextures(ambientTexture, diffuseTexture, specularTexture, normalTexture);

        shader
            .setShininess(material.shininess())
            // make sure ambient alpha is 1.0 since it gets multiplied with instanced vertex color
            .setAmbientColor({ material.ambientColor().rgb(), 1.0f })
            // diffuse and specular colors have no alpha so when the light influences are added up, alpha is unaffected
            // this isn't ideal since specular highlights should increase opacity, but with the built-in Phong shader
            // the alpha added from a single strong light makes the entire object completely opaque
            // TODO maybe this is fixable with some light value tweaks
            .setDiffuseColor({ material.diffuseColor().rgb(), 0.0f })
            .setSpecularColor({ material.specularColor().rgb(), 0.0f })
            .setProjectionMatrix(camera.projectionMatrix());

        shader.draw(_mesh);
    }

    Magnum::Shaders::Phong& shader;
    Magnum::UnsignedInt _meshId;
    Magnum::GL::Mesh& _mesh;
    Magnum::GL::Buffer& instanceBuffer;
    const Magnum::Trade::PhongMaterialData& material;

    Magnum::GL::Texture2D *ambientTexture, *diffuseTexture, *specularTexture, *normalTexture;

    Magnum::SceneGraph::DrawableGroup3D instanceDrawables;
    typename InstanceDrawable<Transform>::InstanceArray instanceData;
};
