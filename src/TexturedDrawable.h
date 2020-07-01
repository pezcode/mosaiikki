#pragma once

#include "ColoredDrawable.h"
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/GL/Texture.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>

template<typename Transform>
class TexturedDrawable : public ColoredDrawable<Transform>
{
public:
    explicit TexturedDrawable(
        Object3D& object,
        Magnum::Shaders::Phong& shader,
        Magnum::GL::Mesh& mesh,
        Corrade::Containers::ArrayView<Corrade::Containers::Optional<Magnum::GL::Texture2D>> textures,
        Magnum::UnsignedInt textureOffset,
        const Magnum::Trade::PhongMaterialData& material,
        const Magnum::Color4& color = Magnum::Color4(1.0f, 1.0f, 1.0f)) :
        ColoredDrawable(object, shader, mesh, color),
        textures(textures),
        textureOffset(textureOffset),
        material(material)
    {
    }

    TexturedDrawable(const TexturedDrawable& other, Object3D& object) :
        ColoredDrawable(object, other.shader, other.mesh, other.color),
        textures(other.textures),
        textureOffset(other.textureOffset),
        material(other.material)
    {
    }

private:
    virtual void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& camera) override
    {
        if(shader.flags() & Magnum::Shaders::Phong::Flag::AmbientTexture &&
           material.flags() & Magnum::Trade::PhongMaterialData::Flag::AmbientTexture)
            shader.bindAmbientTexture(*textures[textureOffset + material.ambientTexture()]);

        if(shader.flags() & Magnum::Shaders::Phong::Flag::DiffuseTexture &&
           material.flags() & Magnum::Trade::PhongMaterialData::Flag::DiffuseTexture)
            shader.bindDiffuseTexture(*textures[textureOffset + material.diffuseTexture()]);

        if(shader.flags() & Magnum::Shaders::Phong::Flag::NormalTexture &&
           material.flags() & Magnum::Trade::PhongMaterialData::Flag::NormalTexture)
            shader.bindNormalTexture(*textures[textureOffset + material.normalTexture()]);

        if(shader.flags() & Magnum::Shaders::Phong::Flag::SpecularTexture &&
           material.flags() & Magnum::Trade::PhongMaterialData::Flag::SpecularTexture)
            shader.bindSpecularTexture(*textures[textureOffset + material.specularTexture()]);

        shader.setDiffuseColor(color)
            .setTransformationMatrix(transformationMatrix)
            .setNormalMatrix(transformationMatrix.normalMatrix())
            .setProjectionMatrix(camera.projectionMatrix());

        shader.draw(mesh);
    }

    const Magnum::Trade::PhongMaterialData& material;
    Corrade::Containers::ArrayView<Corrade::Containers::Optional<Magnum::GL::Texture2D>> textures;
    Magnum::UnsignedInt textureOffset;
};
