#pragma once

#include "AnimationEnums.h"

struct AnimDescriptor
{
	AnimationId AnimId;
	int32 Flags;

	AnimDescriptor() : AnimId(ANIM_ID_UNDEFINED), Flags(0) {}
	AnimDescriptor(AnimationId id, int32 flags) : AnimId(id), Flags(flags) {}
};

struct AnimAssocDefinition
{
	static const size_t ANIM_NAME_BUF_SZ = 24;

	char GroupName[16];
	char BlockName[16];
	int32 ModelIndex;
	int32 NumAnims;

	const char **AnimNames; //!< Pointers to heap allocated char arrays (size 24 each) - The array of pointers itself is heap allocated as well - Size == animsCount
	AnimDescriptor *AnimDescr;
};

VALIDATE_SIZE(AnimAssocDefinition, 0x30);
