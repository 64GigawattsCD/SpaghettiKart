#pragma once
#include <libultraship/libultraship.h>

namespace GameUI {
void SetupGuiElements();
void Destroy();
extern std::shared_ptr<Ship::GuiWindow> mLanMultiplayerWindow;
} // namespace GameUI

class GameMenuBar : public Ship::GuiMenuBar {
  public:
    using Ship::GuiMenuBar::GuiMenuBar;

  protected:
    void DrawElement() override;
    void InitElement() override {};
    void UpdateElement() override {};
};
