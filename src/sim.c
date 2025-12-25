#include "sim.h"
#include <errno.h>

int sim_run(const sim_params_t *params, results_t *out) {
    (void)params; (void)out;
    errno = ENOSYS;
    return -1;
}

