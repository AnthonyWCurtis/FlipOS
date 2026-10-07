#include "flip_optimizer_pmu_token.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_round_trip(void)
{
    uint32_t token = 0;
    assert(flip_optimizer_pmu_token_encode_trial(
               "cpu-1800000-1037500-1",
               &token) == 1);
    assert(token != 0);

    char trial[128];
    assert(flip_optimizer_pmu_token_decode_trial(
               token,
               trial,
               sizeof(trial)) == 1);
    assert(strcmp(
               trial,
               "cpu-1800000-1037500-1") == 0);
}

static void test_all_opps(void)
{
    const char *trials[] = {
        "cpu-408000-837500-1",
        "cpu-600000-837500-2",
        "cpu-816000-837500-3",
        "cpu-1104000-837500-4",
        "cpu-1416000-912500-5",
        "cpu-1608000-987500-6",
        "cpu-1800000-1037500-7",
        "cpu-1992000-850000-21",
    };

    for (size_t i = 0;
         i < sizeof(trials) / sizeof(trials[0]);
         ++i) {
        uint32_t token = 0;
        assert(flip_optimizer_pmu_token_encode_trial(
                   trials[i],
                   &token) == 1);

        char decoded[128];
        assert(flip_optimizer_pmu_token_decode_trial(
                   token,
                   decoded,
                   sizeof(decoded)) == 1);
        assert(strcmp(decoded, trials[i]) == 0);
    }
}

static void test_phase_round_trip(void)
{
    const char *trial = "cpu-1800000-1037500-1";
    uint32_t token = 0;
    char decoded[128];
    flip_optimizer_pmu_phase_t phase;

    assert(flip_optimizer_pmu_token_encode_trial_phase(
               trial,
               FLIP_OPTIMIZER_PMU_PHASE_PREPARED,
               &token) == 1);
    assert(flip_optimizer_pmu_token_decode_trial_phase(
               token,
               decoded,
               sizeof(decoded),
               &phase) == 1);
    assert(strcmp(decoded, trial) == 0);
    assert(phase == FLIP_OPTIMIZER_PMU_PHASE_PREPARED);

    assert(flip_optimizer_pmu_token_encode_trial_phase(
               trial,
               FLIP_OPTIMIZER_PMU_PHASE_ENTERING,
               &token) == 1);
    assert(flip_optimizer_pmu_token_decode_trial_phase(
               token,
               decoded,
               sizeof(decoded),
               &phase) == 1);
    assert(strcmp(decoded, trial) == 0);
    assert(phase == FLIP_OPTIMIZER_PMU_PHASE_ENTERING);
}

static void test_legacy_v1_decodes_as_entering(void)
{
    /*
     * V1 token for cpu-1800000-1037500-1. V1 had no explicit phase,
     * so backward-compatible recovery must treat it conservatively as entered.
     */
    const uint32_t legacy = 0xf175304cU;
    char decoded[128];
    flip_optimizer_pmu_phase_t phase;

    assert(flip_optimizer_pmu_token_decode_trial_phase(
               legacy,
               decoded,
               sizeof(decoded),
               &phase) == 1);
    assert(strcmp(
               decoded,
               "cpu-1800000-1037500-1") == 0);
    assert(phase == FLIP_OPTIMIZER_PMU_PHASE_ENTERING);
}

static void test_rejects_bad_tokens(void)
{
    uint32_t token = 0;
    assert(flip_optimizer_pmu_token_encode_trial(
               "acceptance-deadman-001",
               &token) == 0);
    assert(flip_optimizer_pmu_token_encode_trial(
               "cpu-1800000-1037501-1",
               &token) == 0);
    assert(flip_optimizer_pmu_token_encode_trial(
               "cpu-1800000-1037500-64",
               &token) == 0);

    char trial[128];
    assert(flip_optimizer_pmu_token_decode_trial(
               0,
               trial,
               sizeof(trial)) == 0);
    assert(flip_optimizer_pmu_token_decode_trial(
               0xf1400001U,
               trial,
               sizeof(trial)) == -2);
}

int main(void)
{
    test_round_trip();
    test_all_opps();
    test_phase_round_trip();
    test_legacy_v1_decodes_as_entering();
    test_rejects_bad_tokens();

    printf("flip_optimizer_pmu_token tests passed\n");
    return 0;
}
