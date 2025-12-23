#define PERSIST_H

#include "common.h"

int persist_save(const char *path, const sim_params_t *params, const results_t *res);
int persist_load(const char *path, sim_params_t *out_params, results_t *out_res);
