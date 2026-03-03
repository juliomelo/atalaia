#pragma once

#include <csignal>

#define REMOVE_FILES
#define KEEP_PACKAGES_AFTER_KEYFRAME
#undef SHOW_MOVEMENT_DETECTION
#undef SHOW_OBJECT_DETECTION
#undef SHOW_OBJECT_DETECTION_NET
#define USE_TRACKER
#undef SHOW_FPS
#undef VERBOSE
#define MOVEMENT_PERIOD_WATCH 3.0

extern volatile sig_atomic_t terminating;
