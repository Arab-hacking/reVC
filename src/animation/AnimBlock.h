#pragma once

#include "AnimationEnums.h"

class CAnimBlock
{
public:
	char Name[16];
	bool IsLoaded;
	int16 RefCnt;
	int32 FirstAnimIdx;
	uint32 NumAnims;
	AssocGroupId GroupId;
};

VALIDATE_SIZE(CAnimBlock, 0x20);
