/*
 * Copyright (c) 2023 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_timer.h Vehicle SOA timer.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_TIMER_H
#define VSOA_TIMER_H

#include <stdint.h>
#include <stdbool.h>

/* This module provides timer function to facilitate timing business processing.
 * First call the `vsoa_timer_init` function for initialization. After initialization, 
 * vsoa will create a timer service thread, and all timer callback functions
 * will be executed in this thread context, so the timer callback function
 * should be simple, otherwise it will block the execution of subsequent timer callbacks 
 * VSOA timer API is thread safe */

#ifdef __cplusplus
extern "C" {
#endif

/* Structure declaration */
struct vsoa_timer;

/* Timer type */
typedef struct vsoa_timer vsoa_timer_t;

/* Timer on time callback
 * The callback function will be called in the timer service thread context */
typedef void (*vsoa_timer_ontime_func_t)(/* void *arg, vsoa_timer_t *timer, void *custom */);

/* Initialize timer service 
 * The initialization function must first be called */
bool vsoa_timer_init(void);

/* VSOA timer create */
vsoa_timer_t *vsoa_timer_create(void);

/* VSOA timer delete */
void vsoa_timer_delete(vsoa_timer_t *timer);

/* VSOA timer start
 * The time unit is nanoseconds, if `interval` is 0, it means trigger once */
bool vsoa_timer_start(vsoa_timer_t *timer, int64_t count, int64_t interval, vsoa_timer_ontime_func_t callback, void *arg);

/* VSOA timer start
 * The time unit is millisecond, if `interval` is 0, it means trigger once */
bool vsoa_timer_start_ms(vsoa_timer_t *timer, int64_t count, int64_t interval, vsoa_timer_ontime_func_t callback, void *arg);

/* VSOA timer stop */
void vsoa_timer_stop(vsoa_timer_t *timer);

/* Is the timer working */
bool vsoa_timer_is_started(vsoa_timer_t *timer);

/* VSOA timer set custom data */
void vsoa_timer_set_custom(vsoa_timer_t *timer, void *custom);

/* VSOA timer get custom data */
void *vsoa_timer_custom(vsoa_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif /* VSOA_TIMER_H */
/*
 * end
 */
