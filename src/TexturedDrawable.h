#pragma once

#include "ColoredDrawable.h"
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/GL/Texture.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Assert.h>

template<typename Transform>
class TexturedDrawable : public ColoredDrawable<Transform>
{
public:
    typedef typename ColoredDrawable<Transform>::Object3D Object3D;

    explicit TexturedDrawable(
        Object3D& object,
        Magnum::Shaders::Phong& shader,
        Magnum::GL::Mesh& mesh,
        Corrade::Containers::ArrayView<Corrade::Containers::Optional<Magnum::GL::Texture2D>> textures,
        const Magnum::Trade::PhongMaterialData& material) :
        ColoredDrawable<Transform>(object, shader, mesh), textures(textures), material(material)
    {
        CORRADE_ASSERT(shader.flags() & (Magnum::Shaders::Phong::Flag::AmbientTexture |
                                         Magnum::Shaders::Phong::Flag::DiffuseTexture |
                                         Magnum::Shaders::Phong::Flag::SpecularTexture |
                                         Magnum::Shaders::Phong::Flag::NormalTexture),
                       "Phong shader must support at least one texture type", );
    }

    explicit TexturedDrawable(const TexturedDrawable& other, Object3D& object) :
        ColoredDrawable<Transform>(other, object), textures(other.textures), material(other.material)
    {
    }

private:
    virtual void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& camera) override
    {
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
            .setDiffuseColor(this->color)
            .setShininess(this->shininess)
            .setTransformationMatrix(transformationMatrix)
            .setNormalMatrix(transformationMatrix.normalMatrix())
            .setProjectionMatrix(camera.projectionMatrix());

        this->shader.draw(this->mesh);
    }

    Corrade::Containers::ArrayView<Corrade::Containers::Optional<Magnum::GL::Texture2D>> textures;
    const Magnum::Trade::PhongMaterialData& material;
};
