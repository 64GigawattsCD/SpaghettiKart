#pragma once
#include <libultraship/libultraship.h>
namespace GameUI {
class LanRaceWindow : public Ship::GuiWindow {
public: using Ship::GuiWindow::GuiWindow;
protected:
    void InitElement() override {}
    void DrawElement() override;
    void UpdateElement() override {}
};
}
