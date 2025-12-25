#include "persist.h"
#include <errno.h>

int persist_save(const char *path, const sim_params_t *params, const results_t *res) {
    (void)path; (void)params; (void)res;
    errno = ENOSYS;
    return -1;
}

int persist_load(const char *path, sim_params_t *out_params, results_t *out_res) {
    (void)path; (void)out_params; (void)out_res;
    errno = ENOSYS;
    return -1;
}

