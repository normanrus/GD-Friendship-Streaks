#pragma once
// Capability switches for the message hooks.
//
// GD 2.2091 stopped binding GJWriteMessagePopup outside Android: `create` has no
// address on Windows or macOS, and on Windows the upload callback is unbound as
// well. 2.2081 binds everything on every platform, and Android still binds the
// whole class on both versions.

#if defined(GEODE_IS_ANDROID) || !defined(GEODE_COMP_GD_VERSION) || GEODE_COMP_GD_VERSION < 22091
    #define FSTREAK_CAN_OPEN_WRITE_POPUP 1
#else
    #define FSTREAK_CAN_OPEN_WRITE_POPUP 0
#endif

#if defined(GEODE_IS_WINDOWS) && defined(GEODE_COMP_GD_VERSION) && GEODE_COMP_GD_VERSION >= 22091
    #define FSTREAK_CAN_HOOK_MESSAGE 0
#else
    #define FSTREAK_CAN_HOOK_MESSAGE 1
#endif

// The friends list row lost its macOS address in 2.2091, so the flame next to
// the name cannot be drawn there.
#if defined(GEODE_IS_MACOS) && defined(GEODE_COMP_GD_VERSION) && GEODE_COMP_GD_VERSION >= 22091
    #define FSTREAK_CAN_HOOK_USER_CELL 0
#else
    #define FSTREAK_CAN_HOOK_USER_CELL 1
#endif
