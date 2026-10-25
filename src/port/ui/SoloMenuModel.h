#pragma once

namespace GameUI::SoloMenu {
constexpr bool Enabled(bool explicitSolo, bool localOnly, int localCount) {
    return explicitSolo && localOnly && localCount == 1;
}

constexpr bool ShowCapacity(bool explicitSolo) { return !explicitSolo; }

constexpr int RowsPerPage = 6;

// These helpers are for solo setup only; settingCount is the persistent NEXT footer.
constexpr int ClampSelection(int selected, int settingCount) {
    return selected < 0 || settingCount < 0 ? 0 : selected > settingCount ? settingCount : selected;
}

constexpr int MoveSelection(int selected, int direction, int settingCount) {
    const int current = ClampSelection(selected, settingCount);
    if (direction < 0 && current > 0) return current - 1;
    if (direction > 0 && current < settingCount) return current + 1;
    return current;
}

constexpr int PageFirst(int selected, int settingCount) {
    const int current = ClampSelection(selected, settingCount);
    const int row = current == settingCount && current > 0 ? current - 1 : current;
    return row / RowsPerPage * RowsPerPage;
}

enum class PendingStage { None, OpenSetup, OpenDrivers, Launch };

struct PendingFlow {
    PendingStage stage = PendingStage::None;

    void Reset() { stage = PendingStage::None; }

    void Queue(PendingStage next, bool enabled) {
        stage = enabled ? next : PendingStage::None;
    }

    // Parent resets on Back, queues OpenDrivers on retry, and readies before Launch.
    // OpenSetup follows an explicit standings SETUP request and NET_RETURN_LOBBY.
    // Non-Lobby polls wait for preparation; consume before dispatching the returned action.
    PendingStage Take(bool enabled, bool inLobby, bool canStart, bool interrupted = false) {
        if (!enabled || interrupted) { Reset(); return PendingStage::None; }
        if (!inLobby || (stage == PendingStage::Launch && !canStart)) return PendingStage::None;
        const auto action = stage;
        Reset();
        return action;
    }
};
} // namespace GameUI::SoloMenu
