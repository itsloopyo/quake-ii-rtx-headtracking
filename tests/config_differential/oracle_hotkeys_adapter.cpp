// Compiled into the hotkey oracle library only, with `cameraunlock` renamed, so the poller is
// oracle_fake's and chord_hotkeys.h is the dev build's core copy (oracle/core/include).
//
// The dev build registered its hotkeys in Mod::RegisterHotkeys (src/core/mod.cpp at b6665cf),
// which cannot be compiled apart from the whole mod. Its six AddHotkey lines are copied below
// unchanged but for the receiver: `m_hotkeys` is the fake poller, `m_config.key*` the three
// codes, and each Mod::Instance() action counts into `fired`.
#include "cameraunlock/input/hotkey_poller.h"
#include "cameraunlock/input/chord_hotkeys.h"
#include "oracle_adapter.h"

namespace q2_oracle_view {

FireTable OracleFires(int keyToggle, int keyTogglePosition, int keyToggleYaw) {
    namespace input = cameraunlock::input;
    std::array<int, kActions> fired{};
    struct {
        std::array<int, kActions>* fired;
        void Toggle() { ++(*fired)[0]; }
        void CycleMode() { ++(*fired)[1]; }
        void ToggleYawMode() { ++(*fired)[2]; }
    } mod{&fired};
    auto* m = &mod;
    input::FakeRegistrations().clear();
    {
        input::HotkeyPoller m_hotkeys;
        struct {
            int keyToggle, keyTogglePosition, keyToggleYaw;
        } m_config{keyToggle, keyTogglePosition, keyToggleYaw};

        using namespace cameraunlock::input;
        m_hotkeys.AddHotkey(m_config.keyToggle, NavGuarded([m] { m->Toggle(); }));
        m_hotkeys.AddHotkey(m_config.keyTogglePosition, NavGuarded([m] { m->CycleMode(); }));
        m_hotkeys.AddHotkey(m_config.keyToggleYaw, NavGuarded([m] { m->ToggleYawMode(); }));

        m_hotkeys.AddHotkey('Y', ChordGuarded([m] { m->Toggle(); }));
        m_hotkeys.AddHotkey('G', ChordGuarded([m] { m->CycleMode(); }));
        m_hotkeys.AddHotkey('H', ChordGuarded([m] { m->ToggleYawMode(); }));
    }
    const std::vector<input::FakeRegistration> registered = input::FakeRegistrations();

    // The dev build's poller: a callback runs when its key goes down.
    FireTable table;
    table.reserve((kLastKey - kFirstKey + 1) * kHeldStates);
    for (int vk = kFirstKey; vk <= kLastKey; ++vk) {
        for (int held = 0; held < kHeldStates; ++held) {
            fired = {};
            input::FakeHeld() = held;
            for (const input::FakeRegistration& r : registered) {
                if (r.vk == vk && r.callback) r.callback();
            }
            table.push_back(fired);
        }
    }
    input::FakeHeld() = 0;
    return table;
}

}  // namespace q2_oracle_view
