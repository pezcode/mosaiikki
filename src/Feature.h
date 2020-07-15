#pragma once

#include <Magnum/SceneGraph/Object.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/GrowableArray.h>

namespace Feature
{
    template<typename F> using FeatureList = Corrade::Containers::Array<F*>;

// find first feature of the specified type
template<typename F, typename T>
F* feature(Magnum::SceneGraph::Object<T>& object)
{
    for(auto& candidate : object.features())
    {
        F* feature = dynamic_cast<F*>(&candidate);
        if(feature)
        {
            return feature;
        }
    }
    return nullptr;
}

// find all features of the specified type
template<typename F, typename T>
FeatureList<F> features(Magnum::SceneGraph::Object<T>& object)
{
    FeatureList<F> result;
    for(auto& candidate : object.features())
    {
        F* feature = dynamic_cast<F*>(&candidate);
        if(feature)
        {
            Corrade::Containers::arrayAppend(result, feature);
        }
    }
    return result;
}

// find all features of the specified type in any direct or indirect children
template<typename F, typename T>
FeatureList<F> featuresInChildren(Magnum::SceneGraph::Object<T>& object, FeatureList<F>* result = nullptr)
{
    FeatureList<F> storage;
    if(result == nullptr)
    {
        result = &storage;
    }
    else
    {
        for(auto& candidate : object.features())
        {
            F* feature = dynamic_cast<F*>(&candidate);
            if(feature)
            {
                Corrade::Containers::arrayAppend(*result, feature);
            }
        }
    }

    for(auto& child : object.children())
    {
        featuresInChildren(child, result);
    }

    return storage;
}

// find all features of the specified type in any direct or indirect parent
template<typename F, typename T>
FeatureList<F> featuresInParents(Magnum::SceneGraph::Object<T>& object, FeatureList<F>* result = nullptr)
{
    FeatureList<F> storage;
    if(result == nullptr)
    {
        result = &storage;
    }
    else
    {
        for(auto& candidate : object.features())
        {
            F* feature = dynamic_cast<F*>(&candidate);
            if(feature)
            {
                Corrade::Containers::arrayAppend(*result, feature);
            }
        }
    }
    Magnum::SceneGraph::Object<T>* parent = object.parent();
    if(parent != nullptr)
    {
        featuresInParents(object.parent(), result);
    }
    return storage;
}
} // namespace Feature
