#pragma once

#include "ColoredDrawableInstanced.h"
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/GL/Texture.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Assert.h>

template<typename Transform>
class TexturedDrawableInstanced : public ColoredDrawableInstanced<Transform>
{
public:
    explicit TexturedDrawableInstanced(
        Object3D& object,
        Magnum::Shaders::Phong& shader,
        Magnum::UnsignedInt meshId,
        Magnum::GL::Mesh& mesh,
        Magnum::GL::Buffer& instanceBuffer,
        Corrade::Containers::ArrayView<Corrade::Containers::Optional<Magnum::GL::Texture2D>> textures,
        const Magnum::Trade::PhongMaterialData& material) :
        ColoredDrawableInstanced<Transform>(object, shader, meshId, mesh, instanceBuffer),
        textures(textures),
        material(material)
    {
        CORRADE_ASSERT(shader.flags() & (Magnum::Shaders::Phong::Flag::AmbientTexture |
                                         Magnum::Shaders::Phong::Flag::DiffuseTexture |
                                         Magnum::Shaders::Phong::Flag::SpecularTexture |
                                         Magnum::Shaders::Phong::Flag::NormalTexture),
                       "Phong shader must support at least one texture type", );
    }

    explicit TexturedDrawableInstanced(const TexturedDrawableInstanced& other, Object3D& object) :
        ColoredDrawableInstanced<Transform>(other, object), textures(other.textures), material(other.material)
    {
    }

private:
    virtual void draw(const Magnum::Matrix4& /*transformationMatrix*/, Magnum::SceneGraph::Camera3D& camera) override
    {
        if(_instanceDrawables.isEmpty())
            return;

        Magnum::GL::Texture2D* ambient = material.flags() & Magnum::Trade::PhongMaterialData::Flag::AmbientTexture
                                             ? &*textures[material.ambientTexture()]
                                             : nullptr;
        Magnum::GL::Texture2D* diffuse = material.flags() & Magnum::Trade::PhongMaterialData::Flag::DiffuseTexture
                                             ? &*textures[material.diffuseTexture()]
                                             : nullptr;
        Magnum::GL::Texture2D* specular = material.flags() & Magnum::Trade::PhongMaterialData::Flag::SpecularTexture
                                              ? &*textures[material.specularTexture()]
                                              : nullptr;
        Magnum::GL::Texture2D* normal = material.flags() & Magnum::Trade::PhongMaterialData::Flag::NormalTexture
                                            ? &*textures[material.normalTexture()]
                                            : nullptr;

        // can't access templated base class's members without this->

        this->shader.bindTextures(ambient, diffuse, specular, normal)
            .setShininess(this->shininess)
            .setProjectionMatrix(camera.projectionMatrix());

        Corrade::Containers::arrayResize(this->_instanceData, 0);
        camera.draw(this->_instanceDrawables);

        this->instanceBuffer.setData(this->_instanceData, GL::BufferUsage::DynamicDraw);
        this->_mesh.setInstanceCount(this->_instanceData.size());

        this->shader.draw(this->_mesh);
    }

    Corrade::Containers::ArrayView<Corrade::Containers::Optional<Magnum::GL::Texture2D>> textures;
    const Magnum::Trade::PhongMaterialData& material;
};
