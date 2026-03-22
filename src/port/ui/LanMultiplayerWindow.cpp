#include "LanMultiplayerWindow.h"

#include "port/net/NetworkManager.h"

#include <imgui.h>
#include <spdlog/spdlog.h>

namespace GameUI {

LanMultiplayerWindow::~LanMultiplayerWindow() {
    SPDLOG_TRACE("destruct lan multiplayer window");
}

void LanMultiplayerWindow::InitElement() {
}

void LanMultiplayerWindow::DrawElement() {
    static char sessionName[64] = "Spaghetti Kart LAN";
    auto& network = Network::NetworkManager::Instance();

    ImGui::TextWrapped("Initial LAN multiplayer scaffolding. This currently supports local discovery, ENet host/client "
                       "transport setup, and a basic connection handshake.");
    ImGui::Separator();

    ImGui::InputText("Session Name", sessionName, IM_ARRAYSIZE(sessionName));
    ImGui::Text("Status: %s", network.GetStatusText().c_str());
    ImGui::Text("Game Port: %u", network.GetGamePort());

    if (!network.IsHostingLan()) {
        if (ImGui::Button("Host LAN Session", ImVec2(180, 0))) {
            network.StartLanHost(sessionName);
        }
    } else {
        if (ImGui::Button("Stop Hosting", ImVec2(180, 0))) {
            network.StopLanHost();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh LAN Servers", ImVec2(180, 0))) {
        network.RefreshLanServers();
    }

    if (network.IsConnected()) {
        ImGui::SameLine();
        if (ImGui::Button("Disconnect", ImVec2(120, 0))) {
            network.Disconnect();
        }
    }

    ImGui::Separator();
    if (ImGui::BeginTable("LanServers", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Session");
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Port");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();

        for (const auto& server : network.GetLanServers()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(server.SessionName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(server.Address.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%u", server.GamePort);
            ImGui::TableNextColumn();

            if (server.IsSelf) {
                ImGui::TextUnformatted("Hosting");
            } else {
                const std::string buttonId = "Join##" + server.Address + ":" + std::to_string(server.GamePort);
                if (ImGui::Button(buttonId.c_str())) {
                    network.JoinLanServer(server.Address, server.GamePort);
                }
            }
        }

        ImGui::EndTable();
    }
}

} // namespace GameUI
