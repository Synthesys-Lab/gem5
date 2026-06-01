#ifndef LLPM_GEM5_VERILATOR_ABI_H
#define LLPM_GEM5_VERILATOR_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LLPM_GEM5_VERILATOR_ABI_VERSION 1u

typedef enum LlpmComponentId {
    LLPM_COMPONENT_RTL_DCACHE = 1,
    LLPM_COMPONENT_RTL_SPLIT_CACHE = 2,
    LLPM_COMPONENT_RTL_MINOR_PIPELINE = 3,
    LLPM_COMPONENT_RTL_PHT2_BPRED = 4,
} LlpmComponentId;

typedef enum LlpmOperation {
    LLPM_OP_CACHE_LOAD = 1,
    LLPM_OP_CACHE_STORE = 2,
    LLPM_OP_BPRED_QUERY = 3,
    LLPM_OP_BPRED_UPDATE = 4,
    LLPM_OP_PIPELINE_RESET = 5,
    LLPM_OP_PIPELINE_STEP = 6,
    LLPM_OP_IFETCH_REQUEST = 7,
    LLPM_OP_DATA_LOAD_REQUEST = 8,
    LLPM_OP_DATA_STORE_REQUEST = 9,
    LLPM_OP_MEMORY_RESPONSE = 10,
    LLPM_OP_HALT = 11,
    LLPM_OP_FAULT = 12,
    LLPM_OP_INTERRUPT = 13,
} LlpmOperation;

typedef struct LlpmComponentConfig {
    uint32_t abi_version;
    uint32_t component_id;
    uint32_t reset_cycles;
    uint32_t flags;
    const char *json_config;
} LlpmComponentConfig;

typedef struct LlpmComponentRequest {
    uint64_t request_id;
    uint32_t operation;
    uint32_t size;
    uint64_t addr;
    uint64_t data;
    uint64_t byte_mask;
    uint64_t metadata0;
    uint64_t metadata1;
    uint64_t metadata2;
    uint64_t metadata3;
} LlpmComponentRequest;

typedef struct LlpmComponentResponse {
    uint64_t request_id;
    uint32_t ok;
    uint32_t operation;
    uint64_t data;
    uint32_t hit_valid;
    uint32_t hit;
    uint64_t latency_cycles;
    int32_t error_code;
    uint32_t reserved;
    uint64_t metadata0;
    uint64_t metadata1;
    uint64_t metadata2;
    uint64_t metadata3;
} LlpmComponentResponse;

typedef struct LlpmComponentStat {
    const char *name;
    double value;
    uint64_t value_u64;
    uint32_t flags;
} LlpmComponentStat;

typedef struct LlpmComponentTrace {
    uint64_t cycle;
    uint64_t request_id;
    uint32_t event;
    uint32_t operation;
    uint64_t metadata0;
    uint64_t metadata1;
    uint64_t metadata2;
    uint64_t metadata3;
} LlpmComponentTrace;

uint32_t llpm_component_abi_version(void);
uint32_t llpm_component_id(void);
const char *llpm_component_name(void);
void *llpm_component_create(const LlpmComponentConfig *config);
void llpm_component_destroy(void *handle);
int32_t llpm_component_reset(void *handle, uint32_t reset_cycles);
int32_t llpm_component_submit(
    void *handle,
    const LlpmComponentRequest *request
);
int32_t llpm_component_step(void *handle, uint32_t cycles);
int32_t llpm_component_pop_response(
    void *handle,
    LlpmComponentResponse *response
);
uint32_t llpm_component_stat_count(void *handle);
int32_t llpm_component_stat(
    void *handle,
    uint32_t index,
    LlpmComponentStat *stat
);
uint32_t llpm_component_trace_count(void *handle);
int32_t llpm_component_trace(
    void *handle,
    uint32_t index,
    LlpmComponentTrace *trace
);

#ifdef __cplusplus
}
#endif

#endif
