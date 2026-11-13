#ifndef STUB_TIMER_H
#define STUB_TIMER_H
#include "common.h"
class CTimer { public: static float GetTimeStep(void){ return 1.0f; } static float GetTimeStepFix(void){ return 1.0f; } };
#endif
