// NOLINTBEGIN(readability-use-concise-preprocessor-directives)
#ifndef MIRU_EVDEV_KEYS_H
#define MIRU_EVDEV_KEYS_H

#if defined(__linux__)
#include <linux/input-event-codes.h>
#elif defined(__FreeBSD__)
#include <dev/evdev/input-event-codes.h>
#else
#error "no evdev keycode header for this platform"
#endif

#endif // !MIRU_EVDEV_KEYS_H
// NOLINTEND(readability-use-concise-preprocessor-directives)
