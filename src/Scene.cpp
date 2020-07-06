#include "Scene.h"

#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/FunctionsBatch.h>
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

    SceneGraph::Animable3D* animable = new TranslationAnimable3D(cameraObject, Vector3::yAxis(), 3.0f, -5.5f);
    cameraAnimables.add(*animable);
    animable->setState(SceneGraph::AnimationState::Running);

    root.setParent(&scene);

    // Shaders

    coloredMaterialShader = Shaders::Phong(
        Shaders::Phong::Flag::InstancedTransformation | Shaders::Phong::Flag::VertexColor, lightPositions.size());

    texturedMaterialShader =
        Shaders::Phong(Shaders::Phong::Flag::InstancedTransformation | Shaders::Phong::Flag::VertexColor |
                           Shaders::Phong::Flag::DiffuseTexture | Shaders::Phong::Flag::SpecularTexture |
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

    Object3D& object = root.addChild<Object3D>();
    object.translate({ 0.0f, 0.0f, -5.0f });

    Range3D bounds;
    bool loaded = loadScene(mesh.c_str(), object, &bounds);
    CORRADE_ASSERT(loaded, "Failed to load scene", );

    float scale = 2.0f / bounds.size().max();

    object.scaleLocal(Vector3(scale));

    Vector3 center(float(objectGridSize - 1) / 2.0f);

    for(ColoredDrawableInstanced3D* drawable : featuresInChildren<ColoredDrawableInstanced3D>(object))
    {
        for(size_t i = 0; i < objectGridSize; i++)
        {
            for(size_t j = 0; j < objectGridSize; j++)
            {
                for(size_t k = 0; k < objectGridSize; k++)
                {
                    Object3D& drawableObject = static_cast<Object3D&>(drawable->object());
                    Object3D& instance = drawableObject.addChild<Object3D>();

                    Matrix3 toLocal = Matrix3(instance.absoluteTransformationMatrix()).inverted();

                    Vector3 translation = (Vector3(i, j, -float(k)) - center) * 4.0f;
                    instance.translate(toLocal * translation);

                    InstanceDrawable3D* instanceDrawable =
                        new InstanceDrawable3D(instance, drawable->instanceData());
                    instanceDrawable->setColor(Color4(i, j, k) * 1.0f / objectGridSize);
                    drawable->instanceDrawables().add(*instanceDrawable);

                    Vector3 localX = toLocal * Vector3::xAxis();
                    Vector3 localY = toLocal * Vector3::yAxis();

                    SceneGraph::Animable3D* translationAnimable =
                        new TranslationAnimable3D(instance, localX, 3.0f * localX.length(), 5.5f * localX.length());
                    meshAnimables.add(*translationAnimable);
                    SceneGraph::Animable3D* rotationAnimable = new RotationAnimable3D(instance, localY, 90.0_degf);
                    meshAnimables.add(*rotationAnimable);

                    translationAnimable->setState(SceneGraph::AnimationState::Running);
                    rotationAnimable->setState(SceneGraph::AnimationState::Running);
                }
            }
        }
    }
}

bool Scene::loadScene(const char* file, Object3D& parent, Range3D* bounds)
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

    Range3D sceneBounds;

    Containers::arrayResize(meshes, meshes.size() + importer->meshCount());
    Containers::arrayResize(instanceBuffers, instanceBuffers.size() + importer->meshCount());
    for(UnsignedInt i = 0; i < importer->meshCount(); i++)
    {
        Containers::Optional<Trade::MeshData> data = importer->mesh(i);
        if(data && data->hasAttribute(Trade::MeshAttribute::Position) &&
           data->hasAttribute(Trade::MeshAttribute::Normal) &&
           GL::meshPrimitive(data->primitive()) == GL::MeshPrimitive::Triangles)
        {
            if(bounds)
            {
                const std::pair<Vector3, Vector3> minmax = Math::minmax(data->positions3DAsArray());
                const Range3D meshBounds = { minmax.first, minmax.second };
                sceneBounds.min() = Math::min(sceneBounds.min(), meshBounds.min());
                sceneBounds.max() = Math::max(sceneBounds.max(), meshBounds.max());
            }

            GL::Buffer indices, vertices;
            indices.setData(data->indexData());
            vertices.setData(data->vertexData());

            meshes[i] = MeshTools::compile(*data, indices, vertices);
            instanceBuffers[i] = InstanceDrawable3D::addInstancedBuffer(*meshes[i]);
        }
    }

    if(bounds)
        *bounds = sceneBounds;

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
                texture.setMagnificationFilter(textureData->magnificationFilter())
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
                    TexturedDrawableInstanced3D* drawable =
                        new TexturedDrawableInstanced3D(object,
                                                        texturedMaterialShader,
                                                        *meshes[objectData->instance()],
                                                        *instanceBuffers[objectData->instance()],
                                                        textures.suffix(textureOffset),
                                                        material);
                    drawables.add(*drawable);
                    textured = true;
                }
            }

            if(!textured)
            {
                // TODO
                // can we unify textured and colored drawables somehow?
                // maybe with a white default diffuse texture
                // or just don't load color-only meshes
                ColoredDrawableInstanced3D* drawable =
                    new ColoredDrawableInstanced3D(object,
                                                   coloredMaterialShader,
                                                   *meshes[objectData->instance()],
                                                   *instanceBuffers[objectData->instance()]);
                drawables.add(*drawable);

                // by default, there are no instances
                // add a InstanceDrawable3D to drawable->instanceDrawables() for each instance
            }
        }

        for(UnsignedInt childObjectId : objectData->children())
        {
            addObject(importer, childObjectId, object, textureOffset);
        }
    }
}
