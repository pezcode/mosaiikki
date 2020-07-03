#include "Scene.h"

#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/MeshTools/Compile.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/PluginManager/Manager.h>

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace Feature;

Scene::Scene(NoCreateT) : coloredMaterialShader(NoCreate), texturedMaterialShader(NoCreate) { }

Scene::Scene() : coloredMaterialShader(NoCreate), texturedMaterialShader(NoCreate)
{
    // Scene

    lightPositions = Containers::array<Vector3>({ { -3.0f, 10.0f, 10.0f } });
    lightColors = Containers::array<Color4>({ 0xffffff_rgbf });
    CORRADE_INTERNAL_ASSERT(lightPositions.size() == lightColors.size());

    cameraObject.setParent(&scene).translate(Vector3::zAxis(-5.0f));
    camera.reset(new SceneGraph::Camera3D(cameraObject));

    SceneGraph::Animable3D* animable = new Animable3D(
        cameraObject, Vector3::xAxis(), 3.0f, -5.5f); //new Animable3D(cameraObject, Vector3::yAxis(), 1.5f, 1.0f);
    cameraAnimables.add(*animable);
    animable->setState(SceneGraph::AnimationState::Running);

    root.setParent(&scene);

    // Shaders

    coloredMaterialShader = Shaders::Phong(Shaders::Phong::Flag(0), lightPositions.size());

    texturedMaterialShader =
        Shaders::Phong(Shaders::Phong::Flag::DiffuseTexture | Shaders::Phong::Flag::SpecularTexture |
                           Shaders::Phong::Flag::NormalTexture,
                       lightPositions.size());

    coloredMaterialShader.setAmbientColor(0x111111_rgbf);
    coloredMaterialShader.setLightPositions(lightPositions);
    coloredMaterialShader.setLightColors(lightColors);
    coloredMaterialShader.setLabel("Material shader (colored)");

    texturedMaterialShader.setLightPositions(lightPositions);
    coloredMaterialShader.setLightColors(lightColors);
    texturedMaterialShader.setLabel("Material shader (textured)");

    // Objects

    std::string mesh = "resources/models/Suzanne.glb";
    mesh = "resources/models/Avocado/Avocado.gltf";

    Object3D& original = root.addChild<Object3D>();
    bool loaded = loadScene(mesh.c_str(), original);
    CORRADE_ASSERT(loaded, "Failed to load scene", );

    Vector3 center(float(objectGridSize - 1) / 2.0f);

    for(size_t i = 0; i < objectGridSize; i++)
    {
        for(size_t j = 0; j < objectGridSize; j++)
        {
            for(size_t k = 0; k < objectGridSize; k++)
            {
                Object3D& duplicate = duplicateObject(original, *original.parent());
                duplicate.scale(Vector3(50.0f));
                duplicate.translate((Vector3(i, j, -float(k)) - center) * 4.0f);

                for(ColoredDrawable3D* drawable : featuresInChildren<ColoredDrawable3D>(duplicate))
                {
                    drawable->setColor(Color4(i, j, k) * 1.0f / objectGridSize);
                }

                SceneGraph::Animable3D* duplicatedAnimable = new Animable3D(duplicate, Vector3::xAxis(), 3.0f, 5.5f);
                meshAnimables.add(*duplicatedAnimable);
                duplicatedAnimable->setState(SceneGraph::AnimationState::Running);
            }
        }
    }

    // remove original object that was copied in the grid
    for(TexturedDrawable3D* drawable : featuresInChildren<TexturedDrawable3D>(original))
    {
        drawables.remove(*drawable);
    }
}

bool Scene::loadScene(const char* file, Object3D& parent)
{
    // load importer

    PluginManager::Manager<Trade::AbstractImporter> manager;
    Containers::Pointer<Trade::AbstractImporter> importer = manager.loadAndInstantiate("AnySceneImporter");
    if(!importer)
        return false;

    // load scene

    if(!importer->openFile(file))
        return false;

    if(importer->sceneCount() == 0)
        return false;

    Int sceneId = importer->defaultScene();
    if(sceneId == -1)
        sceneId = 0;
    Containers::Optional<Trade::SceneData> sceneData = importer->scene(sceneId);
    if(!sceneData)
        return false;

    // extract and compile meshes

    Containers::arrayResize(meshes, meshes.size() + importer->meshCount());
    for(UnsignedInt i = 0; i < importer->meshCount(); i++)
    {
        Containers::Optional<Trade::MeshData> data = importer->mesh(i);
        if(data && data->hasAttribute(Trade::MeshAttribute::Position) &&
           data->hasAttribute(Trade::MeshAttribute::Normal) &&
           GL::meshPrimitive(data->primitive()) == GL::MeshPrimitive::Triangles)
        {
            meshes[i] = MeshTools::compile(*data);
        }
    }

    // load materials

    Containers::arrayResize(materials, materials.size() + importer->materialCount());
    for(UnsignedInt i = 0; i < importer->materialCount(); i++)
    {
        Containers::Pointer<Trade::AbstractMaterialData> data = importer->material(i);
        if(data && data->type() == Trade::MaterialType::Phong)
        {
            materials[i] = std::move(static_cast<Trade::PhongMaterialData&>(*data));
        }
    }

    // load textures

    Magnum::UnsignedInt textureOffset = textures.size();

    Containers::arrayResize(textures, textures.size() + importer->textureCount());
    for(UnsignedInt i = 0; i < importer->textureCount(); i++)
    {
        Containers::Optional<Trade::TextureData> textureData = importer->texture(i);
        if(textureData && textureData->type() == Trade::TextureData::Type::Texture2D)
        {
            Containers::Optional<Trade::ImageData2D> imageData = importer->image2D(textureData->image(), 0 /* level */);
            if(imageData)
            {
                GL::TextureFormat format;
                switch(imageData->format())
                {
                    case PixelFormat::RGB8Unorm:
                        format = GL::TextureFormat::RGB8;
                        break;
                    case PixelFormat::RGBA8Unorm:
                        format = GL::TextureFormat::RGB8;
                        break;
                    default:
                        continue;
                }
                GL::Texture2D texture;
                texture
                    .setMagnificationFilter(textureData->magnificationFilter())
                    .setMinificationFilter(textureData->minificationFilter(), textureData->mipmapFilter())
                    .setWrapping(textureData->wrapping().xy())
                    .setStorage(Math::log2(imageData->size().max()) + 1, format, imageData->size())
                    .setSubImage(0, {}, *imageData)
                    .generateMipmap();
                textures[i] = std::move(texture);
            }
        }
    }

    // add objects recursively

    for(UnsignedInt objectId : sceneData->children3D())
    {
        addObject(*importer, objectId, parent, textureOffset);
    }

    return true;
}

void Scene::addObject(Trade::AbstractImporter& importer,
                      UnsignedInt objectId,
                      Object3D& parent,
                      Magnum::UnsignedInt textureOffset)
{
    // meshes are compiled and accessible at this point

    Containers::Pointer<Trade::ObjectData3D> objectData = importer.object3D(objectId);
    if(objectData)
    {
        Object3D& object = parent.addChild<Object3D>();
        object.setTransformation(objectData->transformation());

        if(objectData->instanceType() == Trade::ObjectInstanceType3D::Mesh && objectData->instance() != -1 &&
           meshes[objectData->instance()])
        {
            bool textured = false;
            const Int materialId = static_cast<Trade::MeshObjectData3D*>(objectData.get())->material();
            if(materialId != -1 && materials[materialId])
            {
                const Trade::PhongMaterialData& material = *materials[materialId];
                constexpr Trade::PhongMaterialData::Flags texturedFlags =
                    Trade::PhongMaterialData::Flag::DiffuseTexture | Trade::PhongMaterialData::Flag::SpecularTexture |
                    Trade::PhongMaterialData::Flag::NormalTexture;

                if((material.flags() & texturedFlags) == texturedFlags)
                {
                    SceneGraph::Drawable3D* drawable = new TexturedDrawable3D(object,
                                                                              texturedMaterialShader,
                                                                              *meshes[objectData->instance()],
                                                                              textures.suffix(textureOffset),
                                                                              material);
                    drawables.add(*drawable);
                    textured = true;
                }
            }

            if(!textured)
            {
                SceneGraph::Drawable3D* drawable =
                    new ColoredDrawable3D(object, coloredMaterialShader, *meshes[objectData->instance()]);
                drawables.add(*drawable);
            }
        }

        for(UnsignedInt childObjectId : objectData->children())
        {
            addObject(importer, childObjectId, object, textureOffset);
        }
    }
}

Scene::Object3D& Scene::duplicateObject(Object3D& object, Object3D& parent)
{
    CORRADE_INTERNAL_ASSERT(!object.isScene());

    Object3D& duplicate = parent.addChild<Object3D>();
    duplicate.setTransformation(object.transformation());

    TexturedDrawable3D* texturedDrawable = feature<TexturedDrawable3D>(object);
    if(texturedDrawable)
    {
        TexturedDrawable3D* newDrawable = new TexturedDrawable3D(*texturedDrawable, duplicate);
        drawables.add(*newDrawable);
    }

    ColoredDrawable3D* coloredDrawable = feature<ColoredDrawable3D>(object);
    if(coloredDrawable)
    {
        ColoredDrawable3D* newDrawable = new ColoredDrawable3D(*coloredDrawable, duplicate);
        drawables.add(*newDrawable);
    }

    for(Object3D& child : object.children())
    {
        duplicateObject(child, duplicate);
    }

    return duplicate;
}
