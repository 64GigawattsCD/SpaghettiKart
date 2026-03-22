#pragma once

#include <libultraship/libultraship.h>

namespace GameUI {

class LanMultiplayerWindow : public Ship::GuiWindow {
  public:
    using Ship::GuiWindow::GuiWindow;
    ~LanMultiplayerWindow();

  protected:
    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override {}
};

} // namespace GameUI
