#pragma once

#include <Magnum/Trade/MaterialData.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Packing.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>

class DefaultMaterial : public Magnum::Trade::MaterialData
{
public:
    enum : Magnum::UnsignedInt
    {
        AmbientTextureId = 0u,
        DiffuseTextureId,
        SpecularTextureId,
        NormalTextureId,
        TextureCount
    };

    explicit DefaultMaterial() :
        MaterialData(
            Magnum::Trade::MaterialType::Phong,
            { { Magnum::Trade::MaterialAttribute::AmbientTexture, Magnum::UnsignedInt(AmbientTextureId) },
              { Magnum::Trade::MaterialAttribute::DiffuseTexture, Magnum::UnsignedInt(DiffuseTextureId) },
              { Magnum::Trade::MaterialAttribute::SpecularTexture, Magnum::UnsignedInt(SpecularTextureId) },
              { Magnum::Trade::MaterialAttribute::SpecularGlossinessTexture, Magnum::UnsignedInt(SpecularTextureId) },
              { Magnum::Trade::MaterialAttribute::NormalTexture, Magnum::UnsignedInt(NormalTextureId) },
              { Magnum::Trade::MaterialAttribute::Shininess, 80.0f } })
    {
    }

    Corrade::Containers::Array<Magnum::GL::Texture2D> createTextures(Magnum::Vector2i size = { 1, 1 })
    {
        Corrade::Containers::Array<Magnum::GL::Texture2D> textures(Corrade::NoInit, TextureCount);
        // NoInit requires placement new
        new(&textures[AmbientTextureId]) Magnum::GL::Texture2D(createTexture(Magnum::Color4(1.0f), size));
        new(&textures[DiffuseTextureId]) Magnum::GL::Texture2D(createTexture(Magnum::Color4(1.0f), size));
        new(&textures[SpecularTextureId]) Magnum::GL::Texture2D(createTexture(Magnum::Color4(1.0f), size));
        new(&textures[NormalTextureId]) Magnum::GL::Texture2D(createTexture(Magnum::Color4(0.5f, 0.5f, 1.0f), size));

        return textures;
    };

private:
    Magnum::GL::Texture2D createTexture(const Magnum::Color4& color, Magnum::Vector2i size)
    {
        Corrade::Containers::Array<Magnum::Color4ub> data(
            Corrade::DirectInit, size.product(), Magnum::Math::pack<Magnum::Color4ub>(color));
        Magnum::ImageView2D image { Magnum::PixelFormat::RGBA8Unorm, size, data };

        Magnum::GL::Texture2D texture;

        texture.setStorage(1, Magnum::GL::TextureFormat::RGBA8, size);
        texture.setSubImage(0, { 0, 0 }, image);

        return texture;
    }
};
