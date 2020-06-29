#pragma once

#include <Magnum/SceneGraph/Object.h>
#include <vector>

namespace Feature
{
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
Corrade::Containers::LinkedList<F> features(Magnum::SceneGraph::Object<T>& object)
{
    Corrade::Containers::LinkedList<F> list;
    for(auto& candidate : object.features())
    {
        F* feature = dynamic_cast<F*>(&candidate);
        if(feature)
        {
            list.insert(feature);
        }
    }
    return list;
}

// find all features of the specified type in any direct or indirect children
template<typename F, typename T>
std::vector<F*> featuresInChildren(Magnum::SceneGraph::Object<T>& object, std::vector<F*>* list = nullptr)
{
    std::vector<F*>* result;
    if(list == nullptr)
    {
        // initial object, don't add features
        // TODO this is a memory leak
        result = new std::vector<F*>();
    }
    else
    {
        result = list;
        for(auto& candidate : object.features())
        {
            F* feature = dynamic_cast<F*>(&candidate);
            if(feature)
            {
                result->push_back(feature);
            }
        }
    }
    for(auto& child : object.children())
    {
        featuresInChildren(child, result);
    }
    return *result;
}

// find all features of the specified type in any direct or indirect parent
template<typename F, typename T>
std::vector<F*> featuresInParents(Magnum::SceneGraph::Object<T>& object, std::vector<F*>* list = nullptr)
{
    std::vector<F*>* result;
    if(list == nullptr)
    {
        result = new std::vector<F*>();
    }
    else
    {
        result = list;
        for(auto& candidate : object.features())
        {
            F* feature = dynamic_cast<F*>(&candidate);
            if(feature)
            {
                result->push_back(feature);
            }
        }
    }
    Magnum::SceneGraph::Object<T>* parent = object.parent();
    if(parent != nullptr)
    {
        featuresInParents(object.parent(), result);
    }
    return *result;
}
} // namespace Feature
