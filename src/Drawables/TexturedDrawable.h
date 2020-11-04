#pragma once

#include "Drawables/InstanceDrawable.h"
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/AbstractObject.h>
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
//#include <algorithm>

template<typename Transform>
class TexturedDrawable : public Magnum::SceneGraph::Drawable3D
{
public:
    typedef Magnum::SceneGraph::AbstractObject<Transform::Dimensions, typename Transform::Type> Object;

    explicit TexturedDrawable(
        Object& object,
        Magnum::Shaders::Phong& shader,
        Magnum::UnsignedInt meshId,
        Magnum::GL::Mesh& mesh,
        Magnum::GL::Buffer& instanceBuffer,
        Corrade::Containers::ArrayView<Corrade::Containers::Pointer<Magnum::GL::Texture2D>> textures,
        const Magnum::Trade::PhongMaterialData& material,
        Magnum::Float shininess) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(shader),
        _meshId(meshId),
        _mesh(mesh),
        instanceBuffer(instanceBuffer),
        material(material),
        shininess(shininess)
    {
        ambientTexture = diffuseTexture = specularTexture = normalTexture = nullptr;

        CORRADE_ASSERT(isCompatibleMaterial(material, shader), "Material missing required textures", );

        if(material.hasAttribute(Magnum::Trade::MaterialAttribute::AmbientTexture))
            ambientTexture = textures[material.ambientTexture()].get();
        if(material.hasAttribute(Magnum::Trade::MaterialAttribute::DiffuseTexture))
            diffuseTexture = textures[material.diffuseTexture()].get();
        if(material.hasAttribute(Magnum::Trade::MaterialAttribute::SpecularTexture) ||
           material.hasAttribute(Magnum::Trade::MaterialAttribute::SpecularGlossinessTexture))
            specularTexture = textures[material.specularTexture()].get();
        if(material.hasAttribute(Magnum::Trade::MaterialAttribute::NormalTexture))
            normalTexture = textures[material.normalTexture()].get();
    }

    Magnum::UnsignedInt meshId() const
    {
        return _meshId;
    }

    InstanceDrawable<Transform>& addInstance(Object& object)
    {
        // template is required so the compiler knows we don't mean less-than
        InstanceDrawable<Transform>& instance = object.template addFeature<InstanceDrawable<Transform>>(instanceData);
        instanceDrawables.add(instance);
        return instance;
    }

    Magnum::SceneGraph::DrawableGroup3D& instances()
    {
        return instanceDrawables;
    }

    static bool isCompatibleMaterial(const Magnum::Trade::PhongMaterialData& material,
                                     const Magnum::Shaders::Phong& shader)
    {
        const std::pair<Magnum::Shaders::Phong::Flag, Magnum::Trade::MaterialAttribute> combinations[] = {
            { Magnum::Shaders::Phong::Flag::AmbientTexture, Magnum::Trade::MaterialAttribute::AmbientTexture },
            { Magnum::Shaders::Phong::Flag::DiffuseTexture, Magnum::Trade::MaterialAttribute::DiffuseTexture },
            // TODO make this more generic, this only works for the GLTF test meshes
            { Magnum::Shaders::Phong::Flag::SpecularTexture,
              Magnum::Trade::MaterialAttribute::SpecularGlossinessTexture },
            { Magnum::Shaders::Phong::Flag::NormalTexture, Magnum::Trade::MaterialAttribute::NormalTexture }
        };

        for(const auto& combination : combinations)
        {
            if((shader.flags() & combination.first) && !material.hasAttribute(combination.second))
                return false;
        }

        return true;
    }

private:
    virtual void draw(const Magnum::Matrix4& /*transformationMatrix*/, Magnum::SceneGraph::Camera3D& camera) override
    {
        if(instanceDrawables.isEmpty())
            return;

        /*
        // sort objects back to front for correct alpha blending
        // not needed currently since we add instances in our scene in the correct order and the camera position is static
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
            // we override the material shininess because for imported GLTF it's always 80
            .setShininess(shininess)
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
    const Magnum::Float shininess;

    Magnum::GL::Texture2D *ambientTexture, *diffuseTexture, *specularTexture, *normalTexture;

    Magnum::SceneGraph::DrawableGroup3D instanceDrawables;
    typename InstanceDrawable<Transform>::InstanceArray instanceData;
};
