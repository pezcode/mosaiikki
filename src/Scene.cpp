#include "Scene.h"

#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Constants.h>
#include <Magnum/Math/FunctionsBatch.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/MeshTools/Compile.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/PluginManager/Manager.h>

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace Corrade;
using namespace Feature;

Scene::Scene(NoCreateT) : materialShader(NoCreate), velocityShader(NoCreate) {}

Scene::Scene() : materialShader(NoCreate), velocityShader(NoCreate)
{
    // Default material

    Containers::Array<GL::Texture2D> defaultTextures = defaultMaterial.createTextures({ 4, 4 });
    for(GL::Texture2D& texture : defaultTextures)
        Containers::arrayAppend(textures, Containers::pointer<GL::Texture2D>(std::move(texture)));

    // Scene

    lightPositions = Containers::array<Vector3>({ { -3.0f, 10.0f, 10.0f } });
    lightColors = Containers::array<Color4>({ 0xffffff_rgbf });
    CORRADE_INTERNAL_ASSERT(lightPositions.size() == lightColors.size());

    cameraObject.setParent(&scene).translate(Vector3::zAxis(-5.0f));
    camera.reset(new SceneGraph::Camera3D(cameraObject));

    SceneGraph::Animable3D* animable = new RotationAnimable3D(cameraObject, Vector3::yAxis(), 15.0_degf, 45.0_degf);
    cameraAnimables.add(*animable);
    animable->setState(SceneGraph::AnimationState::Running);

    root.setParent(&scene);

    // Shaders

    // vertex color is coming from the instance buffer attribute
    materialShader = Shaders::Phong(Shaders::Phong::Flag::InstancedTransformation | Shaders::Phong::Flag::VertexColor |
                                        Shaders::Phong::Flag::DiffuseTexture | Shaders::Phong::Flag::SpecularTexture |
                                        Shaders::Phong::Flag::NormalTexture,
                                    lightPositions.size());
    materialShader.setLightPositions(lightPositions);
    materialShader.setLightColors(lightColors);
    materialShader.setLabel("Material shader (instanced, textured Phong)");

    velocityShader = VelocityShader(VelocityShader::Flag::InstancedTransformation);
    velocityShader.setLabel("Velocity shader (instanced)");

    // Objects

    std::string mesh = "resources/models/Avocado/Avocado.gltf";

    Object3D& object = root.addChild<Object3D>();
    object.translate({ 0.0f, 0.0f, -5.0f });

    Range3D bounds;
    bool loaded = loadScene(mesh.c_str(), object, &bounds);
    CORRADE_ASSERT(loaded, "Failed to load scene", );

    float scale = 2.0f / bounds.size().max();

    object.scaleLocal(Vector3(scale));

    Vector3 center(float(objectGridSize - 1) / 2.0f);

    // animated objects + associated velocity drawables

    for(TexturedDrawable3D* drawable : featuresInChildren<TexturedDrawable3D>(object))
    {
        Object3D& drawableObject = static_cast<Object3D&>(drawable->object());

        size_t id = drawable->meshId();

        VelocityDrawable3D* velocityDrawable = new VelocityDrawable3D(
            drawableObject, velocityShader, id, *velocityMeshes[id], *velocityInstanceBuffers[id]);
        velocityDrawables.add(*velocityDrawable);

        VelocityDrawable3D* transparentVelocityDrawable = new VelocityDrawable3D(
            drawableObject, velocityShader, id, *velocityMeshes[id], *velocityInstanceBuffers[id]);
        transparentVelocityDrawables.add(*transparentVelocityDrawable);

        for(size_t z = 0; z < objectGridSize; z++)
        {
            for(size_t y = 0; y < objectGridSize; y++)
            {
                for(size_t x = 0; x < objectGridSize; x++)
                {
                    Object3D& instance = drawableObject.addChild<Object3D>();

                    Matrix3 toLocal = Matrix3(instance.absoluteTransformationMatrix()).inverted();

                    // add instances back to front for alpha blending
                    Vector3 translation = (Vector3(x, y, -float(objectGridSize - z - 1)) - center) * 4.0f;
                    instance.translate(toLocal * translation);

                    InstanceDrawable3D& instanceDrawable = drawable->addInstance(instance);

                    bool transparent = z == (objectGridSize - 1);
                    Color3 color =
                        (Color3(x, y, z) + Color3(1.0f)) / objectGridSize; // +1 to avoid completely black objects
                    float alpha = transparent ? 0.5f : 1.0f;
                    instanceDrawable.setColor(Color4(color, alpha));

                    Vector3 localX = toLocal * Vector3::xAxis();
                    Vector3 localY = toLocal * Vector3::yAxis();

                    if(transparent)
                        transparentVelocityDrawable->addInstance(instance);
                    else
                        velocityDrawable->addInstance(instance);

                    SceneGraph::Animable3D* translationAnimable =
                        new TranslationAnimable3D(instance, localX, 5.5f * localX.length(), 3.0f * localX.length());
                    meshAnimables.add(*translationAnimable);
                    SceneGraph::Animable3D* rotationAnimable =
                        new RotationAnimable3D(instance, localY, 90.0_degf, Rad(Constants::inf()));
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

    Magnum::UnsignedInt meshOffset = meshes.size();
    Containers::arrayResize(meshes, meshes.size() + importer->meshCount());
    Containers::arrayResize(instanceBuffers, instanceBuffers.size() + importer->meshCount());
    Containers::arrayResize(velocityMeshes, velocityMeshes.size() + importer->meshCount());
    Containers::arrayResize(velocityInstanceBuffers, velocityInstanceBuffers.size() + importer->meshCount());

    Range3D sceneBounds;

    for(UnsignedInt i = 0; i < importer->meshCount(); i++)
    {
        Containers::Optional<Trade::MeshData> data = importer->mesh(i);
        if(data && data->hasAttribute(Trade::MeshAttribute::Position) &&
           data->hasAttribute(Trade::MeshAttribute::Normal) && data->hasAttribute(Trade::MeshAttribute::Tangent) &&
           data->hasAttribute(Trade::MeshAttribute::TextureCoordinates) &&
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

            meshes[meshOffset + i].emplace(MeshTools::compile(*data, indices, vertices));
            instanceBuffers[meshOffset + i].emplace(InstanceDrawable3D::addInstancedBuffer(*meshes[i]));

            velocityMeshes[meshOffset + i].emplace(MeshTools::compile(*data, indices, vertices));
            velocityInstanceBuffers[meshOffset + i].emplace(
                VelocityInstanceDrawable3D::addInstancedBuffer(*velocityMeshes[i]));
        }
        else
            Warning(Warning::Flag::NoSpace)
                << "Skipping mesh " << i << " (must be a triangle mesh with normals, tangents and UV coordinates)";
    }

    if(bounds)
        *bounds = sceneBounds;

    // load materials

    Magnum::UnsignedInt materialOffset = materials.size();
    Containers::arrayResize(materials, materials.size() + importer->materialCount());

    for(UnsignedInt i = 0; i < importer->materialCount(); i++)
    {
        Containers::Pointer<Trade::AbstractMaterialData> data = importer->material(i);
        if(data && data->type() == Trade::MaterialType::Phong)
        {
            materials[materialOffset + i].emplace(std::move(static_cast<Trade::PhongMaterialData&>(*data)));
        }
        else
            Warning(Warning::Flag::NoSpace) << "Skipping material " << i << " (not a Phong-compatible material)";
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
                        format = GL::TextureFormat::RGBA8;
                        break;
                    default:
                        Warning(Warning::Flag::NoSpace)
                            << "Skipping texture: "
                            << i //importer->textureName(i) <- TODO, this is empty, how to get the texture path?
                            << " (unsupported format " << imageData->format() << ")";
                        continue;
                }
                GL::Texture2D texture;
                texture.setMagnificationFilter(textureData->magnificationFilter())
                    .setMinificationFilter(textureData->minificationFilter(), textureData->mipmapFilter())
                    .setWrapping(textureData->wrapping().xy())
                    .setStorage(Math::log2(imageData->size().max()) + 1, format, imageData->size())
                    .setSubImage(0, {}, *imageData)
                    .generateMipmap();
                textures[textureOffset + i].emplace(std::move(texture));
            }
        }
    }

    // add objects recursively

    for(UnsignedInt objectId : sceneData->children3D())
    {
        addObject(*importer, objectId, parent, meshOffset, materialOffset, textureOffset);
    }

    return true;
}

void Scene::addObject(Trade::AbstractImporter& importer,
                      UnsignedInt objectId,
                      Object3D& parent,
                      Magnum::UnsignedInt meshOffset,
                      Magnum::UnsignedInt materialOffset,
                      Magnum::UnsignedInt textureOffset)
{
    // meshes are compiled and accessible at this point

    Containers::Pointer<Trade::ObjectData3D> objectData = importer.object3D(objectId);
    if(objectData)
    {
        Object3D& object = parent.addChild<Object3D>();
        object.setTransformation(objectData->transformation());

        if(objectData->instanceType() == Trade::ObjectInstanceType3D::Mesh && objectData->instance() != -1 &&
           meshes[meshOffset + objectData->instance()])
        {
            bool useDefaultMaterial = true;
            const Int materialId = static_cast<Trade::MeshObjectData3D*>(objectData.get())->material();
            if(materialId != -1 && materials[materialOffset + materialId])
            {
                const Trade::PhongMaterialData& material = *materials[materialOffset + materialId];
                const Trade::PhongMaterialData::Flags requiredFlags =
                    TexturedDrawable3D::requiredMaterialFlags(materialShader);
                if((material.flags() & requiredFlags) == requiredFlags)
                {
                    TexturedDrawable3D* drawable =
                        new TexturedDrawable3D(object,
                                               materialShader,
                                               meshOffset + objectData->instance(),
                                               *meshes[meshOffset + objectData->instance()],
                                               *instanceBuffers[meshOffset + objectData->instance()],
                                               textures.suffix(textureOffset),
                                               material);
                    drawables.add(*drawable);
                    useDefaultMaterial = false;
                }
            }

            if(useDefaultMaterial)
            {
                TexturedDrawable3D* drawable =
                    new TexturedDrawable3D(object,
                                           materialShader,
                                           meshOffset + objectData->instance(),
                                           *meshes[meshOffset + objectData->instance()],
                                           *instanceBuffers[meshOffset + objectData->instance()],
                                           textures, // first textures are the default textures
                                           defaultMaterial);
                drawables.add(*drawable);
            }

            // by default, there are no instances
            // add an InstanceDrawable3D to drawable->instanceDrawables() for each instance
        }

        for(UnsignedInt childObjectId : objectData->children())
        {
            addObject(importer, childObjectId, object, textureOffset);
        }
    }
}
