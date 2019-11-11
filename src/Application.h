#pragma once

#include "ImGuiApplication.h"

class Application : public ImGuiApplication
{
public:
    explicit Application(const Arguments& arguments);

private:
    virtual void drawEvent() override;
    virtual void buildUI() override;
};
