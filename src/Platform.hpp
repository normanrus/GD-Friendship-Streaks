#pragma once
// Capability switches for the message hooks.
//
// The mod targets GD 2.2091 only, so the version checks that used to pick between
// the flat message list and the chat threads are gone. What is left are the
// per-platform gaps in the 2.2091 bindings.

// On Windows the message upload callback is not bound in 2.2091, so the hook that
// reports a delivered message is compiled out there.
#if defined(GEODE_IS_WINDOWS)
    #define FSTREAK_CAN_HOOK_MESSAGE 0
#else
    #define FSTREAK_CAN_HOOK_MESSAGE 1
#endif

// The friends list row lost its macOS address in 2.2091, so the flame next to
// the name cannot be drawn there.
#if defined(GEODE_IS_MACOS)
    #define FSTREAK_CAN_HOOK_USER_CELL 0
#else
    #define FSTREAK_CAN_HOOK_USER_CELL 1
#endif

// Only used to name the platform in the log line the mod writes on load.
#if defined(GEODE_IS_ANDROID)
    #define FSTREAK_PLATFORM "android"
#elif defined(GEODE_IS_WINDOWS)
    #define FSTREAK_PLATFORM "windows"
#elif defined(GEODE_IS_MACOS)
    #define FSTREAK_PLATFORM "macos"
#elif defined(GEODE_IS_IOS)
    #define FSTREAK_PLATFORM "ios"
#else
    #define FSTREAK_PLATFORM "unknown"
#endif
