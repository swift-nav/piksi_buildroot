
## Health Daemon
This process offers a framework for creating multiple timer and message event based routines to monitor individual sbp message type streams.

### API

#### Health Monitor Module Organization
The high level approach is to create a self contained module that manages a `health_monitor_t` along with private data that the module needs in order to accomplish its goals.

This is accomplished by creating a set of setup/teardown functions as specified by `health_monitor_init_fn_t` and `health_monitor_deinit_fn_t`.


```C
/* my_health_monitor.h */
int my_health_monitor_init(health_ctx_t *health_ctx);
void my_health_monitor_deinit(void);
```

Where a typical implementation might look like:

```C
/* my_health_monitor.c */
#include "health_monitor.h"
#include "my_health_monitor.h"

static health_monitor_t *my_monitor;

struct {
/*
 some private data wrapped in a context
 */
} private_data_ctx;

...
/*
 some methods to do work on that data here
 */
...

int my_health_monitor_init(health_ctx_t *health_ctx)
{
  my_monitor = health_monitor_create();
  if (my_monitor == NULL) {
    return -1;
  }

  /*
   init private data here
   */

  if (health_monitor_init(my_monitor, health_ctx,
                          SBP_MSG_TYPE, my_monitor_msg_callback,
                          TIMEOUT_PERIOD_MS, my_timeout_callback,
                          (void *)private_data_ctx) != 0) {
    return -1;
  }

  /* indication succes with 0, failure with -1 */
  return 0;
}

void my_health_monitor_deinit(void)
{
  /*
   deinit private data
   */

  health_monitor_destroy(&my_monitor);
}
```

These are manually aggregated in `health_daemon.c` as an array of `health_monitor_init_fn_pairs`, where on execution each init function is called before handing control to the `pk_loop` which performs all of the callback management. On completion, once the `pk_loop` returns, the deinit function is called in order to clean up any memory allocation or other internals of the module.

```C
/* health_daemon.c */
static health_monitor_init_fn_pair_t health_monitor_init_pairs[] = {
  ...
  /* your init/deinit pair must be added to this array */
  {my_health_monitor_init, my_health_monitor_deinit},
  ...
  };
```

#### `health_monitor_init`
Health monitors internals are initialized by this function. It requires an initialized `health_context_t` while other parameters are user configurable in the following way:

* `msg_type` - an SBP message type to subscribe to using the sbp context, if set to zero, no message callback will be registered regardless of the value of `msg_cb`.
* `msg_cb` - if not `NULL` this callback with be executed for every message of `msg_type` received. The return code of this function determines whether or not the internal timer of the `health_context` is reset (0 = reset, 1 = no reset, -1 = error). If set to `NULL` while `msg_type` is valid, the default behavior is to reset the timer with each message received.
* `timer_period` - Time that will elapse before the `timer_cb` is executed. If set to 0, no timer callback will be registered regardless of the value of `timer_cb`.
* `timer_cb` - callback to be executed after timeout occurs. Here `NULL` is not useful for anything as setting the `timer_period` to a non-zero value with a `NULL` callback would result in no activity on timeouts.
* `user_data` - a pointer that can be set to anything the user desires to have returned a `context` in their callbacks.

#### Callbacks
The format of the two callbacks are wrapped in a typedef in `health_monitor.h`, shown here for reference:

```C
typedef int (*health_msg_callback_t) (health_monitor_t *monitor,
                                      u16 sender_id, u8 len, u8 msg[],
                                      void *context);
typedef int (*health_timer_callback_t) (health_monitor_t *monitor,
                                        void *context);
```

In both cases `monitor` is the the monitor initialized when calling `health_monitor_init()` and `context` is the value given for `user_data` for the same function call. For `health_msg_callback_t` the remaining arguements are a direct forwarding of the `sbp_msg_callback_t` defined in libsbp (`libsbp/sbp.h`). As highlighted above, the return value for `health_msg_callback_t` affects the timer behaviour. The return value for `health_timer_callback_t` is currently ignored.
