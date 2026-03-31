 /**
 ******************************************************************************
 * @file    kf.c
 * @author  GPM Application Team
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "kf.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#define kf_dt 1.0f
static const float std_weight_position = 1.f / 20.f;
static const float std_weight_velocity = 1.f / 160.f;

static const float motion_mat[2 * KF_DIM][2 * KF_DIM] = {
  {1, 0, 0, 0, kf_dt, 0,     0,     0},
  {0, 1, 0, 0, 0,     kf_dt, 0,     0},
  {0, 0, 1, 0, 0,     0,     kf_dt, 0},
  {0, 0, 0, 1, 0,     0,     0,     kf_dt},
  {0, 0, 0, 0, 1,     0,     0,     0},
  {0, 0, 0, 0, 0,     1,     0,     0},
  {0, 0, 0, 0, 0,     0,     1,     0},
  {0, 0, 0, 0, 0,     0,     0,     1}
};
static const float motion_mat_t[2 * KF_DIM][2 * KF_DIM] = {
  {1,    0,     0,     0,     0, 0, 0, 0},
  {0,    1,     0,     0,     0, 0, 0, 0},
  {0,    0,     1,     0,     0, 0, 0, 0},
  {0,    0,     0,     1,     0, 0, 0, 0},
  {kf_dt,0,     0,     0,     1, 0, 0, 0},
  {0,    kf_dt, 0,     0,     0, 1, 0, 0},
  {0,    0,     kf_dt, 0,     0, 0, 1, 0},
  {0,    0,     0,     kf_dt, 0, 0, 0, 1}
};
static const float update_mat[KF_DIM][2 * KF_DIM] = {
  {1, 0, 0, 0, 0, 0, 0, 0},
  {0, 1, 0, 0, 0, 0, 0, 0},
  {0, 0, 1, 0, 0, 0, 0, 0},
  {0, 0, 0, 1, 0, 0, 0, 0}
};
static const float update_mat_t[2 * KF_DIM][KF_DIM] = {
  {1, 0, 0, 0},
  {0, 1, 0, 0},
  {0, 0, 1, 0},
  {0, 0, 0, 1},
  {0, 0, 0, 0},
  {0, 0, 0, 0},
  {0, 0, 0, 0},
  {0, 0, 0, 0}
};

#if 0
#include <stdio.h>
static void kf_print_mat(char *header, float *m, int row_nb, int col_nb)
{
  int r, c;

  printf("%s\n", header);
  for (r = 0; r < row_nb; r++) {
    for (c = 0; c < col_nb; c++) {
      printf("%f ", m[r * col_nb + c]);
    }
    printf("\n");
  }
}
#endif

/* result can overlap */
/* result = v * m */
static void kf_vector_mat_dot_product(float *result, float *v, float *m, int row_nb, int col_nb)
{
  float res[row_nb];
  int r, c;

  for (c = 0; c < col_nb; c++) {
    res[c] = 0.f;
    for (r = 0; r < row_nb; r++) {
      res[c] += v[r] * m[r * col_nb + c];
    }
  }

  memcpy(result, res, sizeof(res));
}

/* matrix dot product
 * result = m1 * m2
 * result is a matrix of size row_nb   * col_nb
 * m1     is a matrix of size row_nb   * inter_nb
 * m2     is a matrix of size inter_nb * col_nb
 * result can overlap with either m1 or m2
*/
static void kf_mat_dot_product(float *result, float *m1, float *m2, int row_nb, int col_nb, int inter_nb)
{
  float res[row_nb][col_nb];
  int r, c, i;

  for (r = 0; r < row_nb; r++) {
    for (c = 0; c < col_nb; c++) {
      res[r][c] = 0.f;
      for (i = 0; i < inter_nb; i++) {
        res[r][c] += m1[r * inter_nb + i] * m2[i * col_nb + c];
      }
    }
  }

  memcpy(result, res, sizeof(res));
}

/* matrix dot product with transpose result
 * result = transpose(m1 * m2)
 * result is a matrix of size row_nb   * col_nb
 * m1     is a matrix of size col_nb   * inter_nb
 * m2     is a matrix of size inter_nb * row_nb
 * result can overlap with either m1 or m2
*/
static void kf_mat_dot_product_transpose(float *result, float *m1, float *m2, int row_nb, int col_nb, int inter_nb)
{
  float res[row_nb][col_nb];
  int r, c, i;

  for (r = 0; r < row_nb; r++) {
    for (c = 0; c < col_nb; c++) {
      res[r][c] = 0.f;
      for (i = 0; i < inter_nb; i++) {
        res[r][c] += m1[c * inter_nb + i] * m2[i * row_nb + r];
      }
    }
  }

  memcpy(result, res, sizeof(res));
}

/* matrix transposition
 * result = transpose(m)
 * result is a matrix of size row_nb   * col_nb
 * m      is a matrix of size col_nb   * row_nb
 * both matrix must not overlap
*/
static void kf_mat_transpose(float *result, float *m, int row_nb, int col_nb)
{
  int r, c;

  for (r = 0; r < row_nb; r++) {
    for (c = 0; c < col_nb; c++) {
      result[r * col_nb + c] = m[c * row_nb + r];
    }
  }
}

#if 0
/* matric addition
 * result = m1 + m2
 */
static void kf_mat_add(float *result, float *m1, float *m2, int row_nb, int col_nb)
{
  int r, c;

  for (r = 0; r < row_nb; r++) {
    for (c = 0; c < col_nb; c++) {
      result[r * col_nb + c] = m1[r * col_nb + c] + m2[r * col_nb + c];
    }
  }
}
#endif

/* matric addition
 * result = m1 - m2
 */
static void kf_mat_sub(float *result, float *m1, float *m2, int row_nb, int col_nb)
{
  int r, c;

  for (r = 0; r < row_nb; r++) {
    for (c = 0; c < col_nb; c++) {
      result[r * col_nb + c] = m1[r * col_nb + c] - m2[r * col_nb + c];
    }
  }
}

#if 0
static void kf_cho_decomposition(float *lower, float *A, int row_col_nb)
{
  const int n = row_col_nb;
  float sum;
  int r, c;
  int i;

  memset(lower, 0, row_col_nb * row_col_nb * sizeof(float));

  for (r = 0; r < row_col_nb; r++) {
    for (c = 0; c <= r; c++) {
      sum = 0.f;
      if (r == c) {
        for (i = 0; i < c; i++)
          sum += lower[c * n + i] * lower[c * n + i];
        lower[c * n + c] = sqrtf(A[c * n + c] - sum);
      } else {
        for (i = 0; i < c; i++)
          sum += lower[r * n + i] * lower[c * n + i];
        lower[r * n + c] = (A[r * n + c] - sum) / lower[c * n + c];
      }
    }
  }
}

static void kf_triangular_lower_invert(float *m_inv, float *m, int row_col_nb)
{
  const int n = row_col_nb;
  float sum;
  int r, c;
  int k;

  memset(m_inv, 0, row_col_nb * row_col_nb * sizeof(float));

  for (r = 0; r < row_col_nb; r++) {
    for (c = 0; c <= r; c++) {
      if (r == c) {
        m_inv[r * n + c] = 1.f / m[r * n + c];
      } else {
        sum = 0.f;
        for (k = c; k < r; k++)
          sum += m[r * n + k] * m_inv[k * n + c];
        m_inv[r * n + c] = -sum / m[r * n + r];
      }
    }
  }
}

/* solve A * x = B with A being symetric */
static void kf_cho_solve(float *x, float *A, float *B, float *cho, float *cho_inv, float * cho_inv_t, int row_nb,
                         int col_nb)
{
  kf_cho_decomposition(cho, A, row_nb);
  kf_triangular_lower_invert(cho_inv, cho, row_nb);
  kf_mat_transpose(cho_inv_t, cho_inv, row_nb, row_nb);

  /* use cho for intermediate multiplication */
  kf_mat_dot_product(cho, cho_inv_t, cho_inv, row_nb, row_nb, row_nb);
  kf_mat_dot_product(x, cho, B, row_nb, col_nb, row_nb);
}
#endif

static void kf_project(struct kf_state *state, float projected_mean[KF_DIM], float projected_cov[KF_DIM][KF_DIM])
{
  float innovation_cov[KF_DIM];
  float tmp[KF_DIM][2 * KF_DIM];
  int i;

  innovation_cov[0] = std_weight_position * state->mean[3];
  innovation_cov[1] = std_weight_position * state->mean[3];
  innovation_cov[2] = 1e-1f;
  innovation_cov[3] = std_weight_position * state->mean[3];
  for (i = 0; i < KF_DIM; i ++)
    innovation_cov[i] *= innovation_cov[i];

  /* projected_mean */
  kf_vector_mat_dot_product(projected_mean, (float *) update_mat, state->mean, 2 * KF_DIM, KF_DIM);

  /* projected_cov */
  kf_mat_dot_product((float *) tmp, (float *) update_mat, (float *) state->covariance,
                     KF_DIM, 2 * KF_DIM, 2 * KF_DIM);
  kf_mat_dot_product((float *) projected_cov, (float *) tmp, (float *) update_mat_t,
                     KF_DIM, KF_DIM, 2 * KF_DIM);
  for (i = 0; i < KF_DIM; i++)
    projected_cov[i][i] += innovation_cov[i];
}

void kf_init(struct kf_state *state, struct kf_box *measure)
{
  int i;

  /* init mean */
  state->mean[0] = measure->cx;
  state->mean[1] = measure->cy;
  state->mean[2] = measure->a;
  state->mean[3] = measure->h;
  state->mean[4] = 0.f;
  state->mean[5] = 0.f;
  state->mean[6] = 0.f;
  state->mean[7] = 0.f;

  /* init covariance */
  memset(state->covariance, 0, sizeof(state->covariance));
  state->covariance[0][0] = 2 * std_weight_position * measure->h;
  state->covariance[1][1] = 2 * std_weight_position * measure->h;
  state->covariance[2][2] = 1e-2f;
  state->covariance[3][3] = 2 * std_weight_position * measure->h;
  state->covariance[4][4] = 10 * std_weight_velocity * measure->h;
  state->covariance[5][5] = 10 * std_weight_velocity * measure->h;
  state->covariance[6][6] = 1e-5f;
  state->covariance[7][7] = 10 * std_weight_velocity * measure->h;
  for (i = 0; i < 2 * KF_DIM; i ++)
    state->covariance[i][i] *= state->covariance[i][i];
}

void kf_predict(struct kf_state *state, struct kf_box *predicted)
{
  float motion_cov[2 * KF_DIM];
  int i;

  motion_cov[0] = state->mean[3] * std_weight_position;
  motion_cov[1] = state->mean[3] * std_weight_position;
  motion_cov[2] = 1e-2f;
  motion_cov[3] = state->mean[3] * std_weight_position;
  motion_cov[4] = state->mean[3] * std_weight_velocity;
  motion_cov[5] = state->mean[3] * std_weight_velocity;
  motion_cov[6] = 1e-5f;
  motion_cov[7] = state->mean[3] * std_weight_velocity;
  for (i = 0; i < 2 * KF_DIM; i ++)
    motion_cov[i] *= motion_cov[i];

  /* predict state */
  kf_vector_mat_dot_product(state->mean, state->mean, (float *) motion_mat_t, 2 * KF_DIM, 2 * KF_DIM);

  /*predict covariance : motion_mat * state->covariance * motion_mat_t + motion_cov */
  kf_mat_dot_product((float *) state->covariance, (float *) motion_mat, (float *) state->covariance,
             2 * KF_DIM, 2 * KF_DIM, 2 * KF_DIM);
  kf_mat_dot_product((float *) state->covariance, (float *) state->covariance, (float *) motion_mat_t,
             2 * KF_DIM, 2 * KF_DIM, 2 * KF_DIM);
  for (i = 0; i < 2 * KF_DIM; i ++)
    state->covariance[i][i] += motion_cov[i];

  /* set predicted result */
  predicted->cx = state->mean[0];
  predicted->cy = state->mean[1];
  predicted->a  = state->mean[2];
  predicted->h  = state->mean[3];
}

void kf_update(struct kf_state *state, struct kf_box *measure)
{
  float covariance_temp[2 * KF_DIM][2 * KF_DIM];
  float projected_cov[KF_DIM][KF_DIM];
  float projected_mean[KF_DIM];
  float kalman_gain_T[KF_DIM][2 * KF_DIM];
  float kalman_gain[2 * KF_DIM][KF_DIM];
  float B[KF_DIM][2 * KF_DIM];
  float innovation[KF_DIM];
  float mean_temp[2 * KF_DIM];
  int i;

  kf_project(state, projected_mean, projected_cov);

  /* B = (P * H^T)^T = H * P  (KF_DIM x 2*KF_DIM) */
  kf_mat_dot_product_transpose((float *) B, (float *) state->covariance, (float *) update_mat_t,
                               KF_DIM, 2 * KF_DIM, 2 * KF_DIM);

  /*
   * kalman_gain_T = S^{-1} * B.  S (projected_cov) is always diagonal because
   * all four axes evolve independently (diagonal noise, diagonal-structured
   * motion and measurement matrices), so a per-row scalar division suffices.
   */
  for (i = 0; i < KF_DIM; i++) {
    float inv_diag = 1.f / projected_cov[i][i];
    for (int j = 0; j < 2 * KF_DIM; j++)
      kalman_gain_T[i][j] = B[i][j] * inv_diag;
  }
  kf_mat_transpose((float *) kalman_gain, (float *) kalman_gain_T, 2 * KF_DIM, KF_DIM);

  innovation[0] = measure->cx - projected_mean[0];
  innovation[1] = measure->cy - projected_mean[1];
  innovation[2] = measure->a  - projected_mean[2];
  innovation[3] = measure->h  - projected_mean[3];

  /* update mean */
  kf_mat_dot_product(mean_temp, (float *) kalman_gain, innovation, 2 * KF_DIM, 1, KF_DIM);
  for (i = 0; i < 2 * KF_DIM; i++)
    state->mean[i] += mean_temp[i];

  /* update covariance */
  kf_mat_dot_product((float *) kalman_gain, (float *) kalman_gain, (float *) projected_cov,
                     2 * KF_DIM, KF_DIM, KF_DIM);
  kf_mat_dot_product((float *) covariance_temp, (float *) kalman_gain, (float *) kalman_gain_T,
                     2 * KF_DIM, 2 * KF_DIM, KF_DIM);
  kf_mat_sub((float *) state->covariance, (float *) state->covariance, (float *) covariance_temp,
             2 * KF_DIM, 2 * KF_DIM);
}
