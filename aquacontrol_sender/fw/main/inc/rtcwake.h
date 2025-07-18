#ifndef __RTCWAKE_H_
#define __RTCWAKE_H_

// #define CONFIG_WAKE_UP_TIME 10
#pragma once

#define CONFIG_WAKE_UP_TIME 40

#ifdef __cplusplus
extern "C" {
#endif

void wake_stub_example(void);
void deepsleep(void);

#ifdef __cplusplus
}
#endif

#endif /*__RTCWAKE_H_*/