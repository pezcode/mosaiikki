#pragma once

#include "Drawables/ColoredDrawable.h"
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/GL/Texture.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Utility/Assert.h>

template<typename Transform>
class TexturedDrawable : public ColoredDrawable<Transform>
{
public:
    explicit TexturedDrawable(
        typename ColoredDrawable<Transform>::Object3D& object,
        Magnum::Shaders::Phong& shader,
        Magnum::UnsignedInt meshId,
        Magnum::GL::Mesh& mesh,
        Magnum::GL::Buffer& instanceBuffer,
        Corrade::Containers::ArrayView<Corrade::Containers::Pointer<Magnum::GL::Texture2D>> textures,
        const Magnum::Trade::PhongMaterialData& material) :
        ColoredDrawable<Transform>(object, shader, meshId, mesh, instanceBuffer), material(material)
    {
        ambientTexture = diffuseTexture = specularTexture = normalTexture = nullptr;

        if(material.flags() & Magnum::Trade::PhongMaterialData::Flag::AmbientTexture)
            ambientTexture = textures[material.ambientTexture()].get();
        if(material.flags() & Magnum::Trade::PhongMaterialData::Flag::DiffuseTexture)
            diffuseTexture = textures[material.diffuseTexture()].get();
        if(material.flags() & Magnum::Trade::PhongMaterialData::Flag::SpecularTexture)
            specularTexture = textures[material.specularTexture()].get();
        if(material.flags() & Magnum::Trade::PhongMaterialData::Flag::NormalTexture)
            normalTexture = textures[material.normalTexture()].get();

        CORRADE_ASSERT(!!ambientTexture == !!(shader.flags() & Magnum::Shaders::Phong::Flag::AmbientTexture) &&
                           !!diffuseTexture == !!(shader.flags() & Magnum::Shaders::Phong::Flag::DiffuseTexture) &&
                           !!specularTexture == !!(shader.flags() & Magnum::Shaders::Phong::Flag::SpecularTexture) &&
                           !!normalTexture == !!(shader.flags() & Magnum::Shaders::Phong::Flag::NormalTexture),
                       "Mismatching shader and material texture flags", );
    }

private:
    virtual void draw(const Magnum::Matrix4& /*transformationMatrix*/, Magnum::SceneGraph::Camera3D& camera) override
    {
        // can't access templated base class's members without this->

        if(this->instanceDrawables.isEmpty())
            return;

        this->shader.bindTextures(ambientTexture, diffuseTexture, specularTexture, normalTexture)
            .setShininess(material.shininess())
            .setDiffuseColor(material.diffuseColor())
            .setSpecularColor(material.specularColor())
            .setProjectionMatrix(camera.projectionMatrix());

        Corrade::Containers::arrayResize(this->instanceData, 0);
        camera.draw(this->instanceDrawables);

        this->instanceBuffer.setData(this->instanceData, Magnum::GL::BufferUsage::DynamicDraw);
        this->_mesh.setInstanceCount(this->instanceData.size());

        this->shader.draw(this->_mesh);
    }

    Magnum::GL::Texture2D *ambientTexture, *diffuseTexture, *specularTexture, *normalTexture;
    const Magnum::Trade::PhongMaterialData& material;
};
