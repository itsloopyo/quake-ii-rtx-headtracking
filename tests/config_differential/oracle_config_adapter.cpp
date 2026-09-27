// Compiled into the config oracle library only, with `cameraunlock` and `Q2RTXHT` renamed, so
// "core/config.h" here is the dev build's (oracle/src/core/config.h).
#include "core/config.h"
#include "oracle_adapter.h"

namespace q2_oracle_view {

OracleConfig RunOracle(const std::string& path) {
    Q2RTXHT::Config c;
    OracleConfig o{};
    o.loaded = c.LoadOrCreate(path);
    o.enabled = c.enabled;
    o.udpPort = c.udpPort;
    o.yawSensitivity = c.yawSensitivity;
    o.pitchSensitivity = c.pitchSensitivity;
    o.rollSensitivity = c.rollSensitivity;
    o.invertYaw = c.invertYaw;
    o.invertPitch = c.invertPitch;
    o.invertRoll = c.invertRoll;
    o.localSmoothing = c.localSmoothing;
    o.remoteSmoothing = c.remoteSmoothing;
    o.worldSpaceYaw = c.worldSpaceYaw;
    o.positionEnabled = c.positionEnabled;
    o.posSensX = c.posSensX;
    o.posSensY = c.posSensY;
    o.posSensZ = c.posSensZ;
    o.posLimitX = c.posLimitX;
    o.posLimitY = c.posLimitY;
    o.posLimitZ = c.posLimitZ;
    o.posLimitZBack = c.posLimitZBack;
    o.positionUnitsPerMeter = c.positionUnitsPerMeter;
    o.collisionEnabled = c.collisionEnabled;
    o.collisionStandoff = c.collisionStandoff;
    o.collisionReleaseSmoothing = c.collisionReleaseSmoothing;
    o.keyToggle = c.keyToggle;
    o.keyTogglePosition = c.keyTogglePosition;
    o.keyToggleYaw = c.keyToggleYaw;
    return o;
}

}  // namespace q2_oracle_view
