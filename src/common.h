#define COMMON_H
#include <stdint.h>

typedef enum {
  MODE_INTERACTIVE = 1,
  MODE_SUMMARY = 2
} sim_mode_t;

typedef struct{
  int width;
  int height;
  int reps;
  int k;

  double p_up;
  double p_down;
  double p_left;
  double p_right;

  sim_mode_t mode;
} sim_params_t;

typedef struct {
  int width;
  int height;

  double *avg_steps;
  double *prob_within_k;
} results_t;
