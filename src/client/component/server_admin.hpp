#pragma once
#include <string>

namespace server_admin
{
    // Active only for dedicated processes explicitly launched with a valid manager nonce.
    bool enabled();
    // Called by the script lifecycle on the game thread. False invalidates roster tokens.
    void set_notice_ready(bool ready);
}
