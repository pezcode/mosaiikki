#pragma once

#include <Magnum/Platform/GlfwApplication.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <imgui_internal.h> // PushItemFlag

class ImGuiApplication : public Magnum::Platform::Application
{
public:
    explicit ImGuiApplication(const Arguments& arguments,
                              const Configuration& configuration = Configuration(),
                              const GLConfiguration& glConfiguration = GLConfiguration());
    explicit ImGuiApplication(const Arguments& arguments, Magnum::NoCreateT);
    ~ImGuiApplication();

    void create(const Configuration& configuration = Configuration(),
                const GLConfiguration& glConfiguration = GLConfiguration());
    bool tryCreate(const Configuration& configuration, const GLConfiguration& glConfiguration = GLConfiguration());

protected:
    // user interface size (used for widget positioning)
    Magnum::Vector2 uiSize() const;
    // convenience function that sets the GUI font
    // removes all other fonts and builds the font atlas
    void setFont(const char* fontFile, float pixels);

    // override and build your UI with ImGui:: calls
    // called each frame
    virtual void buildUI() = 0;

    // call this at the end of your derived application's drawEvent()
    virtual void drawEvent() override;

    virtual void viewportEvent(ViewportEvent& event) override;

    // if you override these, make sure to call the base class
    // implementation first and check if the event was already
    // handled using event.isAccepted()
    virtual void keyPressEvent(KeyEvent& event) override;
    virtual void keyReleaseEvent(KeyEvent& event) override;
    virtual void mousePressEvent(MouseEvent& event) override;
    virtual void mouseReleaseEvent(MouseEvent& event) override;
    virtual void mouseMoveEvent(MouseMoveEvent& event) override;
    virtual void mouseScrollEvent(MouseScrollEvent& event) override;
    virtual void textInputEvent(TextInputEvent& event) override;

private:
    void init();

    Magnum::ImGuiIntegration::Context imgui;
};

// helper for disabled widgets
class ImGuiDisabledZone
{
public:
    ImGuiDisabledZone(bool disabled = true) : disabled(disabled)
    {
        if(disabled)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }
    }

    ~ImGuiDisabledZone()
    {
        if(disabled)
        {
            ImGui::PopStyleVar();
            ImGui::PopItemFlag();
        }
    }

private:
    bool disabled;
};
