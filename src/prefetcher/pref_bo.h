#ifndef __PREF_BO__
#define __PREF_BO__

#ifdef __cplusplus
extern "C" {
#endif

#include "globals/global_types.h"
#include "pref_common.h"

#include "../libs/hash_lib.h"

#define GET_INDEX(addr, num_entries) addr % num_entries
#define GET_TAG(addr, num_entries, mask) (addr / num_entries) & mask

typedef struct BO_Bloom_Filter_struct BO_Bloom_Filter;

typedef struct BO_Table_Entry_Struct {
  Flag trained;
  Flag valid;

  Addr last_addr;
  Addr load_addr;
  Addr start_index;
  Addr pref_last_index;
  int  stride;

  Counter train_num;
  Counter pref_sent;
  Counter last_access;  // for lru
} BO_Table_Entry;

// may need more?
typedef struct BO_RR_Table_Entry_Struct {
  //maybe just to tag?
  Addr line_addr;
  Counter cycle_accessed;
  Flag valid;
} BO_RR_Table_Entry;

typedef struct BO_Struct {
  HWP_Info*               hwp_info;
  // static stuff
  CacheLevel              type;
  // training stuff
  Hash_Table *            score_table;
  Flag                    new_phase; // are we starting a new round
  int                     round;    // what round are we on
  int                     train_offset;   // what offset are we testing
  BO_RR_Table_Entry *     rr_table;
  // prefetching
  int                     cur_offset; // what round are we using currently
  int                     offset_idx;
  Flag                    throttle;
  BO_Bloom_Filter *       bloom_array;
  BO_Bloom_Filter *       cur_bloom;
} Pref_BO;

typedef struct {
  Pref_BO* bestoffset_hwp_core_ul1;
  Pref_BO* bestoffset_hwp_core_umlc;
} bestoffset_prefetchers;

/********************************************/

void pref_bo_init(HWP * hwp);

void pref_bo_umlc_pref_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                        uns32 global_hist);
void pref_bo_umlc_miss(uns8 proc_id, Addr line_addr, Addr loadPC,
                          uns32 global_hist);
void pref_bo_umlc_hit(uns8 proc_id, Addr line_addr, Addr loadPC,
                         uns32 global_hist);
void pref_bo_ul1_pref_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                        uns32 global_hist);

void pref_bo_ul1_miss(uns8 proc_id, Addr line_addr, Addr loadPC,
                          uns32 global_hist);
void pref_bo_ul1_hit(uns8 proc_id, Addr line_addr, Addr loadPC,
                         uns32 global_hist);
/*************************************************************/
/* Internal Function */
void init_bo_core(HWP* hwp, Pref_BO* bo_hwp_core);
void pref_bo_train(Pref_BO* bestoffset_hwp, Addr line_addr, uns8 proc_id);

void pref_bo_umlc_pref_line_filled(uns proc_id, Addr line_addr);
void pref_bo_ul1_pref_line_filled(uns proc_id, Addr line_addr);

void pref_bo_emit_prefetch(Pref_BO * bestoffset_hwp, Addr line_addr, Flag is_umlc, uns8 proc_id);

void pref_bo_reset_scores(Pref_BO* bestoffset_hwp);
void pref_bo_insert_to_rr_table(Pref_BO * bestoffset_hwp, Addr line_addr);
Flag pref_bo_access_rr(Pref_BO * bestoffset_hwp, Addr line_addr);

/*************************************************************/
/* Misc functions */

#ifdef __cplusplus
}
#endif
#endif
