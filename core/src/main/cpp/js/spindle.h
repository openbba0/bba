#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Called from Zygote SpecializeCommon (apps and system_server), and from a
 * non-zygote app_process start. system_server is registered as "system_server". */
void spindle_on_specialize(const char* nice_name);

/* Package name matches the process, or this is app_process running as that package's uid. */
int spindle_process_allowed(const char* nice_name);

/* JS rpc.unlock() and the UNLOCK command both land here. */
void spindle_set_unlock(void);
void spindle_reset_unlock(void);
void spindle_wait_unlock(void);

/* One register attempt. Returns 0 when the service accepted this process.
 * Returns -2 when the binder call was rejected and retrying will not help.
 * *pause is 1 when the host asked to spawn, so the caller must wait for UNLOCK. */
int spindle_binder_try(const char* name, int* pause);

/* Bounded background retries after a failed register. Also used when the
 * service binder dies, so a port restart can pick the process up again. */
void spindle_binder_retry(const char* name);

/* Run a JS payload received over TCP. Returns 0 on success. */
int spindle_run_js(const char* source, int source_len);

/* Forward one console line to the attached host. Also safe when no host is attached. */
void spindle_log(const char* text);

/* Call rpc.exports[method] with a JSON request {"m","a"}.
 * Always stores a malloc'd JSON reply in *out when it returns. */
int spindle_rpc_call(const char* json, int json_len, char** out);

#ifdef __cplusplus
}
#endif
