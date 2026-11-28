#pragma once

// SA eFacialExpression (declared here because CEventEditableResponse
// carries one; SA declares it in TaskSimpleFacial.h, which will include
// this header when it is ported).
enum class eFacialExpression
{
	NONE = -1,

	SURPRISEPANIC = 0,
	SURPRISEMILD = 1,
	CURIOSITY = 2,
	ANGER = 3,
	HAPPINESS = 4,
	SADNESS = 5,

	MAX_NUM_EMOTIONS,

	TALKING = 7,
	CHEWING = 8,
};
