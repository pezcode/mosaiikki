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

    Magnum::GL::Mesh& mesh()
    {
        return _mesh;
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
        // can't access templated base class's members without this->

        if(this->instanceDrawables.isEmpty())
            return;

        if(ambientTexture || diffuseTexture || specularTexture || normalTexture)
            this->shader.bindTextures(ambientTexture, diffuseTexture, specularTexture, normalTexture);

        this->shader.setShininess(material.shininess())
            .setAmbientColor(material.ambientColor())
            .setDiffuseColor(material.diffuseColor())
            .setSpecularColor(material.specularColor())
            .setProjectionMatrix(camera.projectionMatrix());

        Corrade::Containers::arrayResize(this->instanceData, 0);
        camera.draw(this->instanceDrawables);

        this->instanceBuffer.setData(this->instanceData, Magnum::GL::BufferUsage::DynamicDraw);
        this->_mesh.setInstanceCount(this->instanceData.size());

        this->shader.draw(this->_mesh);
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
