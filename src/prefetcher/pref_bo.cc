
#ifdef __cplusplus
extern "C" {
#endif
#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"

#include "globals/assert.h"
#include "globals/utils.h"
#include "op.h"

#include "core.param.h"
#include "dcache_stage.h"
#include "debug/debug.param.h"
#include "general.param.h"
#include "libs/cache_lib.h"
#include "libs/hash_lib.h"
#include "libs/list_lib.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "prefetcher/pref_bo.h"
#include "pref_bo.param.h"
#include "prefetcher/l2l1pref.h"
#include "prefetcher/pref.param.h"
#include "prefetcher/pref_common.h"
#include "statistics.h"

#include "../libs/hash_lib.h"

#ifdef __cplusplus
}
#endif

#include <set>
#include "libs/bloom_filter.hpp"

/**************************************************************************************/
/* Macros */
#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_BO, ##args)

/* bloom filter stuff */
// need an array of bloom filters, one for each offset
// for 'inference' use the bloom filter with the highest score
// below way too much HW complexity
// OR index the offsets by the bloom filter
// check which index it matches, then use that offset
typedef struct BO_Bloom_Filter_struct {
  bloom_filter* bloom;
} BO_Bloom_Filter;
bestoffset_prefetchers bestoffset_prefetcher_array;

int potentialBOs[] = {1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16, 18, 20, 24, 25, 27, 30, 32, 36, 40, 45, 48, 50, 54, 60, 64, 72, 75, 80, 81, 90, 96, 100, 108, 120, 125, 128, 135, 144, 150, 160, 162, 180, 192, 200, 216, 225, 240, 243, 250, 256};
//int POTENTIAL_BOS_SIZE = (int)PREF_BO_OFFSET_N; // should be 52

std::set<Addr> RR_table_debug;

void pref_bo_init(HWP* hwp) {
  STAT_EVENT(0, BO_PREF_INIT);
  if(!PREF_BO_ON) 
    return;

  // no prefetchers prefetch to the dcache??
  hwp->hwp_info->enabled = TRUE;
  if(PREF_UMLC_ON) {
    bestoffset_prefetcher_array.bestoffset_hwp_core_umlc        = (Pref_BO*)malloc(sizeof(Pref_BO) * NUM_CORES);
    bestoffset_prefetcher_array.bestoffset_hwp_core_umlc->type  = UMLC;
    init_bo_core(hwp, bestoffset_prefetcher_array.bestoffset_hwp_core_umlc);
  }
  // I don't think we're prefetching to the UL1?
  if(PREF_UL1_ON){
    bestoffset_prefetcher_array.bestoffset_hwp_core_ul1        = (Pref_BO*)malloc(sizeof(Pref_BO) * NUM_CORES);
    bestoffset_prefetcher_array.bestoffset_hwp_core_ul1->type  = UL1;
    init_bo_core(hwp, bestoffset_prefetcher_array.bestoffset_hwp_core_ul1);
  }
}

void init_bo_core(HWP* hwp, Pref_BO* bo_hwp_core) {
  uns8 proc_id;
  STAT_EVENT(0, BO_PREF_INIT_CORE);

  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    bo_hwp_core[proc_id].hwp_info     = hwp->hwp_info;
    bo_hwp_core[proc_id].score_table  = (Hash_Table*)calloc(
      1, sizeof(Hash_Table));
    init_hash_table(bo_hwp_core[proc_id].score_table, "score_table", (int)PREF_BO_OFFSET_N, sizeof(uns));
    bo_hwp_core[proc_id].new_phase = TRUE;
    bo_hwp_core[proc_id].round     = 0;
    bo_hwp_core[proc_id].train_offset = potentialBOs[0];
  
    bo_hwp_core[proc_id].rr_table = (BO_RR_Table_Entry*)calloc(
      PREF_BO_RR_TABLE_N, sizeof(BO_RR_Table_Entry));
    bo_hwp_core[proc_id].cur_offset = potentialBOs[0];
    bo_hwp_core[proc_id].offset_idx = 0;
    // If we don't start with prefetches
    bo_hwp_core[proc_id].throttle = FALSE;
    bloom_parameters bloom_params;
    // bloom filter stuff
    bloom_params.projected_element_count = PREF_BO_BLOOM_ENTRIES;
    bloom_params.false_positive_probability = PREF_BO_FALSE_POSITIVE_PROB;
    bloom_parameters.compute_optimal_parameters();
    bo_hwp_core[proc_id].bloom_filters = (BO_Bloom_Filter*)calloc(PREF_BO_OFFSET_N, sizeof(BO_Bloom_Filters));
    for (int ii=0; ii<(int)PREF_BO_OFFSET_N;i++) {
      bo_hwp_core[proc_id].bloom_filters[ii]->bloom = new bloom_filter(bloom_params);
    }
  }
}

// UMLC
void pref_bo_umlc_pref_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                        uns32 global_hist) {
  if(!PREF_UMLC_ON) return;
  pref_bo_emit_prefetch(&bestoffset_prefetcher_array.bestoffset_hwp_core_umlc[proc_id], line_addr, TRUE, proc_id);
  pref_bo_train(&bestoffset_prefetcher_array.bestoffset_hwp_core_umlc[proc_id], line_addr, proc_id);
  
}

void pref_bo_umlc_miss(uns8 proc_id, Addr line_addr, Addr loadPC, uns32 global_hist) {
  // to do
  if(!PREF_UMLC_ON) return;
  STAT_EVENT(proc_id, PREF_BO_ULMC_MISS);
  pref_bo_emit_prefetch(&bestoffset_prefetcher_array.bestoffset_hwp_core_umlc[proc_id], line_addr, TRUE, proc_id);
  pref_bo_train(&bestoffset_prefetcher_array.bestoffset_hwp_core_umlc[proc_id], line_addr, proc_id);
  pref_bo_insert_to_rr_table(&bestoffset_prefetcher_array.bestoffset_hwp_core_umlc[proc_id], line_addr);
}

void pref_bo_umlc_hit(uns8 proc_id, Addr line_addr, Addr loadPC, uns32 global_hist) {
  // to do
  if(!PREF_UMLC_ON) return;
  pref_bo_emit_prefetch(&bestoffset_prefetcher_array.bestoffset_hwp_core_umlc[proc_id], line_addr, TRUE, proc_id);
  pref_bo_insert_to_rr_table(&bestoffset_prefetcher_array.bestoffset_hwp_core_umlc[proc_id], line_addr);
}

// UL1
void pref_bo_ul1_pref_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                        uns32 global_hist) {
  if(!PREF_UL1_ON) return;
  pref_bo_emit_prefetch(&bestoffset_prefetcher_array.bestoffset_hwp_core_ul1[proc_id], line_addr, FALSE, proc_id);
  pref_bo_train(&bestoffset_prefetcher_array.bestoffset_hwp_core_ul1[proc_id], line_addr, proc_id);
}

void pref_bo_ul1_miss(uns8 proc_id, Addr line_addr, Addr loadPC, uns32 global_hist) {
  if(!PREF_UL1_ON) return;
  pref_bo_emit_prefetch(&bestoffset_prefetcher_array.bestoffset_hwp_core_ul1[proc_id], line_addr, FALSE, proc_id);
  pref_bo_train(&bestoffset_prefetcher_array.bestoffset_hwp_core_ul1[proc_id], line_addr, proc_id);
}

void pref_bo_ul1_hit(uns8 proc_id, Addr line_addr, Addr loadPC, uns32 global_hist) {
  if(!PREF_UL1_ON) return;
  pref_bo_emit_prefetch(&bestoffset_prefetcher_array.bestoffset_hwp_core_ul1[proc_id], line_addr, FALSE, proc_id);
}

void pref_bo_train(Pref_BO* bestoffset_hwp, Addr line_addr, uns8 proc_id) {
  // Train score table
  // reset scores
  if (bestoffset_hwp->new_phase) {
    bestoffset_hwp->new_phase = FALSE;
    bestoffset_hwp->offset_idx = 0;
    bestoffset_hwp->round = 0;
    bestoffset_hwp->cur_bloom = bestoffset_hwp->bloom_array[offset_idx];
    // reset score table
    pref_bo_reset_scores(bestoffset_hwp);
  }

  // Test current offset
  Addr candidateLine = line_addr;
  if (pref_bo_access_rr(bestoffset_hwp, candidateLine)) {
    STAT_EVENT(proc_id, PREF_BO_RR_TABLE_HIT);
    // if found increment score
    STAT_EVENT(proc_id, PREF_BO_OFFSET_INCED_1 + bestoffset_hwp->offset_idx);
    int * score = (int*) hash_table_access(bestoffset_hwp->score_table, bestoffset_hwp->train_offset);
    (*score)++;
    // if we reach maxscore use this as the new offset and start a new learning pphase
    if(PREF_BO_BLOOM_FILTER)
      pref_bo_insert_bloom(bestoffset_hwp, bestoffset_hwp->offset_idx);
    if((*score) >= (int)PREF_BO_MAX_SCORE){
      STAT_EVENT(proc_id, PREF_BO_END_ROUND_MAX_SCORE);
      bestoffset_hwp->cur_offset = bestoffset_hwp->train_offset;
      bestoffset_hwp->new_phase = TRUE;
      STAT_EVENT(proc_id, PREF_BO_OFFSET_USED_1 + bestoffset_hwp->offset_idx);
      if(PREF_BO_BLOOM_FILTER)
        //dereference to make sure copy happens?
        (*bestoffset_hwp->cur_bloom) = (*bestoffset_hwp->bloom_array[bestoffset_hwp->offset_idx)]);
    }
  }
  else {
    STAT_EVENT(proc_id, PREF_BO_RR_TABLE_MISS);
  }
  // if we haven't crossed max score but we've reached the max rounds find best offset
  if(bestoffset_hwp->new_phase == FALSE && bestoffset_hwp->round >= (int)PREF_BO_MAX_ROUNDS) {
    STAT_EVENT(proc_id, PREF_BO_END_ROUND_MAX_ROUNDS);
    bestoffset_hwp->new_phase = TRUE;
    // check for max score
    int best_score = 0;
    int best_offset = 0;
    int offset_idx = 0;

    for (int ii=0; ii<(int)PREF_BO_OFFSET_N; ii++) {
      offset_idx = ii;
      int offset_i = potentialBOs[ii];
      int * score = (int*)hash_table_access(bestoffset_hwp->score_table, offset_i);
      if ((*score) > best_score) {
        offset_idx = ii;
        best_score = *score;
        best_offset = offset_i;
      }
    }
    STAT_EVENT(proc_id, PREF_BO_OFFSET_USED_1 + offset_idx);
    bestoffset_hwp->cur_offset = best_offset;
    if(PREF_BO_BLOOM_FILTER)
        //dereference to make sure copy happens?
        (*bestoffset_hwp->cur_bloom) = (*bestoffset_hwp->bloom_array[offset_idx]);
    if(best_score < (int)PREF_BO_BAD_SCORE) {
      STAT_EVENT(proc_id, PREF_BO_THROTTLE_BAD_SCORE);
      bestoffset_hwp->throttle = TRUE;
    }
  }

  bestoffset_hwp->offset_idx = (bestoffset_hwp->offset_idx + 1) % (int)PREF_BO_OFFSET_N;
  bestoffset_hwp->train_offset = potentialBOs[bestoffset_hwp->offset_idx];
  if(bestoffset_hwp->offset_idx == 0) {
    bestoffset_hwp->round++;
  }
}

void pref_bo_emit_prefetch(Pref_BO * bestoffset_hwp, Addr line_addr, Flag is_umlc, uns8 proc_id) {
  if(bestoffset_hwp->throttle)
    return;
  // check if the line is in the bloom filter, if its not then don't prefetch
  if(PREF_BO_BLOOM_FILTER){
    if(!bestoffset_hwp.cur_bloom->bloom.contains(line_addr))
      return;
  }
  STAT_EVENT(proc_id, BO_PREF_EMITTED);
  if(is_umlc){
    for (int ii=0; ii<(int)PREF_BO_DEGREE; ii++){
      pref_addto_umlc_req_queue(proc_id, (line_addr >> DCACHE_LINE_SIZE) + (ii + 1) * bestoffset_hwp->cur_offset, bestoffset_hwp->hwp_info->id);
    }
  }
  else{
    for (int ii=0; ii<(int)PREF_BO_DEGREE; ii++){
      pref_addto_ul1req_queue(proc_id, (line_addr >> DCACHE_LINE_SIZE) + (ii + 1) * bestoffset_hwp->cur_offset, bestoffset_hwp->hwp_info->id);
    }
  }
}

// this introduces the question how many entries should we have in the bloom filter to throttle
// if we get only ~5 or so should we still throttle? or would bad_score already throttle
// Ya bad score would already throttle this, so the implementation would be more sensetive to bad-score param
void pref_bo_insert_bloom(Pref_BO* bestoffset_hwp, Addr line_addr, int offset_idx) {
  bestoffset_hwp.bloom_filters[offset_idx]->bloom->insert(line_addr);
}
// line inserted into recent requests when prefetched line is inserted into UMLC 
// these are sus, test these
void pref_bo_umlc_pref_line_filled(uns proc_id, Addr line_addr) {
  if(!PREF_UMLC_ON) return;
  //STAT_EVENT(proc_id, PREF_BO_RR_TABLE_INSERT);
  //pref_bo_insert_to_rr_table(&bestoffset_prefetcher_array.bestoffset_hwp_core_umlc[proc_id], line_addr);
}

void pref_bo_ul1_pref_line_filled(uns proc_id, Addr line_addr) {
  if(!PREF_UL1_ON) return;
   //STAT_EVENT(proc_id, PREF_BO_RR_TABLE_INSERT);
  //pref_bo_insert_to_rr_table(&bestoffset_prefetcher_array.bestoffset_hwp_core_ul1[proc_id], line_addr);
}

void pref_bo_insert_to_rr_table(Pref_BO * bestoffset_hwp, Addr line_addr) {
  //Addr base_addr = ((line_addr) >> LOG2(DCACHE_LINE_SIZE)) - bestoffset_hwp->cur_offset;
  Addr base_addr = ((line_addr) >> LOG2(DCACHE_LINE_SIZE)) - bestoffset_hwp->cur_offset;
  Addr rr_idx = (base_addr) % PREF_BO_RR_TABLE_N;
  Addr tag = base_addr & 0x00000000ffffffff;
  bestoffset_hwp->rr_table[rr_idx].line_addr = tag;
  bestoffset_hwp->rr_table[rr_idx].cycle_accessed = cycle_count;
  bestoffset_hwp->rr_table[rr_idx].valid = TRUE;
  RR_table_debug.insert(tag);
  DEBUG(0, "Inserted addr: %llu, tag: %llu\n", base_addr, tag);
}

Flag pref_bo_access_rr(Pref_BO * bestoffset_hwp, Addr line_addr) {
  Addr base_addr = ((line_addr) >> LOG2(DCACHE_LINE_SIZE)) - bestoffset_hwp->train_offset;
  Addr base_addr = (line_addr) >> LOG2(DCACHE_LINE_SIZE);
  Addr rr_idx = (base_addr) % PREF_BO_RR_TABLE_N;
  Addr tag = base_addr & 0x00000000ffffffff;
  DEBUG(0, "Lookup addr: %llu, tag: %llu\n", base_addr, tag);
  if(RR_table_debug.count(tag))
    STAT_EVENT(0, PREF_BO_RR_ADDR_SEEN);
  if (bestoffset_hwp->rr_table[rr_idx].line_addr == tag && bestoffset_hwp->rr_table[rr_idx].valid)
    return TRUE;
  return FALSE;
}

void pref_bo_reset_scores(Pref_BO* bestoffset_hwp) {
  int* score;
  Flag new_entry;
  for (int ii=0; ii<(int)PREF_BO_OFFSET_N; ii++) {
    score = (int*)hash_table_access_create(bestoffset_hwp->score_table, potentialBOs[ii], &new_entry);
    *score = 0;
    // clear bloom filters for new phase
    // need to create a when assigning current bloom filter
    if(PREF_BO_BLOOM_FILTER)
      bestoffset_hwp.bloom_array->bloom.clear();
  }
}





