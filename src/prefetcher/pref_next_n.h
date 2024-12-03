#ifndef __PREF_NEXT_N__
#define __PREF_NEXT_N__

#ifdef __cplusplus
extern "C" {
#endif

#include "globals/global_types.h"
#include "pref_common.h"

#include "../libs/hash_lib.h"

#define GET_INDEX(addr, num_entries) addr % num_entries
#define GET_TAG(addr, num_entries, mask) (addr / num_entries) & mask

typedef struct Next_N_Struct {
  HWP_Info*               hwp_info;
  // static stuff
  CacheLevel              type;
  Flag                    throttle;
} Pref_Next_N;

typedef struct {
  Pref_Next_N* next_n_hwp_core_ul1;
  Pref_Next_N* next_n_hwp_core_umlc;
} next_n_prefetchers;

/********************************************/

void pref_next_n_init(HWP * hwp);

void pref_next_n_umlc_pref_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                        uns32 global_hist);
void pref_next_n_umlc_miss(uns8 proc_id, Addr line_addr, Addr loadPC,
                          uns32 global_hist);
void pref_next_n_umlc_hit(uns8 proc_id, Addr line_addr, Addr loadPC,
                         uns32 global_hist);
void pref_next_n_ul1_pref_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                        uns32 global_hist);

void pref_next_n_ul1_miss(uns8 proc_id, Addr line_addr, Addr loadPC,
                          uns32 global_hist);
void pref_next_n_ul1_hit(uns8 proc_id, Addr line_addr, Addr loadPC,
                         uns32 global_hist);
/*************************************************************/
/* Internal Function */
void init_bo_core(HWP* hwp, Pref_Next_N* bo_hwp_core);
void pref_next_n_train(Pref_Next_N* next_n_hwp, Addr line_addr, uns8 proc_id);

void pref_next_n_umlc_pref_line_filled(uns proc_id, Addr line_addr);
void pref_next_n_ul1_pref_line_filled(uns proc_id, Addr line_addr);

void pref_next_n_emit_prefetch(Pref_Next_N * next_n_hwp, Addr line_addr, Flag is_umlc, uns8 proc_id);

void pref_next_n_reset_scores(Pref_Next_N* next_n_hwp);
void pref_next_n_insert_to_rr_table(Pref_Next_N * next_n_hwp, Addr line_addr);
Flag pref_next_n_access_rr(Pref_Next_N * next_n_hwp, Addr line_addr);

/*************************************************************/
/* Misc functions */

#ifdef __cplusplus
}
#endif
#endif
