
## Health Daemon
This process offers a framework for creating multiple timer and message event based routines to monitor individual sbp message type streams.

###API
The high level approach is to create a self contained module that manages a `health_monitor_t` along with private data that the module needs in order to accomplish its goals. The only external facing interface needed is a set of setup/teardown functions as specified by `health_monitor_init_fn_t` and `health_monitor_deinit_fn_t`. These are manually aggregated in `health_daemon.c` as an array of `health_monitor_init_fn_pairs`, where on execution each init function is called before handing control to the zloop which performs all of the callback management. On completion, once the zloop returns, the deinit function is called in order to clean up any memory allocation or other internals of the module.

