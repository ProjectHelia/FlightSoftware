#include <gtest/gtest.h>
#include "fsm.h"

struct Transition {
    fsm_state_t from;
    fsm_event_t evt;
    fsm_state_t expected;
};

// Makes ctest output readable: "SAFE + evt 2 -> ACTIVE" instead of raw bytes
static std::ostream &operator<<(std::ostream &os, const Transition &t) {
    return os << fsm_state_name(t.from) << " + evt " << t.evt
              << " -> " << fsm_state_name(t.expected);
}

class FsmTransitions : public ::testing::TestWithParam<Transition> { };

TEST_P(FsmTransitions, NextStateIsCorrect) {
    const Transition &t = GetParam();
    EXPECT_EQ(fsm_next(t.from, t.evt), t.expected)
        << fsm_state_name(t.from) << " + event " << t.evt;
}

INSTANTIATE_TEST_SUITE_P(AllTransitions, FsmTransitions, ::testing::Values(
                                                             // INIT
                                                             Transition { FSM_INIT, FSM_EVT_NONE, FSM_INIT }, Transition { FSM_INIT, FSM_EVT_INIT_OK, FSM_SAFE }, Transition { FSM_INIT, FSM_EVT_UNLOCK, FSM_INIT }, // can't skip SAFE
                                                             Transition { FSM_INIT, FSM_EVT_SLEEP, FSM_INIT },
                                                             // SAFE
                                                             Transition { FSM_SAFE, FSM_EVT_NONE, FSM_SAFE }, Transition { FSM_SAFE, FSM_EVT_INIT_OK, FSM_SAFE }, Transition { FSM_SAFE, FSM_EVT_UNLOCK, FSM_ACTIVE }, Transition { FSM_SAFE, FSM_EVT_SLEEP, FSM_SLEEP },
                                                             // ACTIVE
                                                             Transition { FSM_ACTIVE, FSM_EVT_NONE, FSM_ACTIVE }, Transition { FSM_ACTIVE, FSM_EVT_UNLOCK, FSM_ACTIVE }, // repeated unlock is harmless
                                                             Transition { FSM_ACTIVE, FSM_EVT_SLEEP, FSM_SLEEP },
                                                             // SLEEP
                                                             Transition { FSM_SLEEP, FSM_EVT_UNLOCK, FSM_SLEEP }, Transition { FSM_SLEEP, FSM_EVT_INIT_OK, FSM_SLEEP }));

TEST(Fsm, CorruptedStateFailsSafe) {
    EXPECT_EQ(fsm_next(static_cast<fsm_state_t>(42), FSM_EVT_UNLOCK), FSM_SAFE);
}

TEST(Fsm, StateNamesNeverNull) {
    for (int s = 0; s <= FSM_STATE_COUNT; ++s) {
        ASSERT_NE(fsm_state_name(static_cast<fsm_state_t>(s)), nullptr);
    }
    EXPECT_STREQ(fsm_state_name(FSM_ACTIVE), "ACTIVE");
    EXPECT_STREQ(fsm_state_name(static_cast<fsm_state_t>(42)), "UNKNOWN");
}