#pragma once

#include "eBoneTag.h"

class CAnimBlendClumpData;
class AnimBlendFrameData;
class CAnimBlendAssociation;
class CAnimBlendHierarchy;
class CAnimBlendLink;

#include "PedNode.h"

//! RpAnimBlend plugin unique rwID
#define rwID_RPANIMBLENDPLUGIN MAKECHUNKID(rwVENDORID_DEVELOPER, 0xFB)

//! 0x4D58A0 - Convert a ped node to its bone tag
eBoneTag32 ConvertPedNode2BoneTag(ePedNode pedNode);

//! 0x4D56F0 - Convert a bone tag to its bone name
const char *ConvertBoneTag2BoneName(eBoneTag32 boneTag);

//! Get RpAnimBlend data associated with this clump
extern RwInt32 ClumpOffset;
inline CAnimBlendClumpData *RpAnimBlendClumpGetData(RpClump *clump)
{
	return *RWPLUGINOFFSET(CAnimBlendClumpData*, clump, ClumpOffset);
}

//! Attach this plugin to RW
//! @return If the plugin was successfully attached
bool RpAnimBlendPluginAttach();

/*!
 * @unused
 */
CAnimBlendAssociation *RpAnimBlendClumpAddAssociation(RpClump *clump, CAnimBlendAssociation *association, uint32 flags, float startTime, float blendAmount);

//! @brief Get the head link for all animations associated with this clump
CAnimBlendLink &RpAnimBlendClumpGetAssociations(RpClump *clump);

//! @addr 0x4D6BE0
//! @brief Take ownership of all associations of the clump
//! @brief It leaves this clump with no associations
//! @brief The opposite of this function is `RpAnimBlendClumpGiveAssociations`
//! @param clump The clump containing the associations
//! @return The first animation of the clump that has a link to all other animations
CAnimBlendAssociation *RpAnimBlendClumpExtractAssociations(RpClump *clump);

/*!
 * @addr 0x4D6BE0
 * @brief Give ownership of a list of associations to this clump
 * @brief The opposite of this function is `RpAnimBlendClumpExtractAssociations`
 * @param clump The clump to give the ownership of associations to
 */
void RpAnimBlendClumpGiveAssociations(RpClump *clump, CAnimBlendAssociation *associations);

//! @addr 0x4D64A0
//! @brief Fill frame array
void RpAnimBlendClumpFillFrameArray(RpClump *clump, AnimBlendFrameData **outFrameData);

//! @addr 0x4D6400
//! @brief Find FrameData of a bone
AnimBlendFrameData *RpAnimBlendClumpFindBone(RpClump *clump, eBoneTag32 boneTag);

//! @addr 0x4D62A0
//! @brief Find FrameData of a bone
AnimBlendFrameData *RpAnimBlendClumpFindFrame(RpClump *clump, const char *name);

//! @addr 0x4D6370
//! @brief Find FrameData of a bone
AnimBlendFrameData *RpAnimBlendClumpFindFrameFromHashKey(RpClump *clump, uint32 boneNameKey);

//! @addr 0x4D68E0
//! @brief Find animation based on a hierarchy
CAnimBlendAssociation *RpAnimBlendClumpGetAssociation(RpClump *clump, bool, CAnimBlendHierarchy *hierarchy);

//! @addr 0x4D6870
//! @brief Find animation of the given name
CAnimBlendAssociation *RpAnimBlendClumpGetAssociation(RpClump *clump, const char *name);

//! @addr 0x4D68B0
//! @brief Find animation of the given id
CAnimBlendAssociation *RpAnimBlendClumpGetAssociation(RpClump *clump, uint32 animId);

//! @addr 0x4D15E0
//! @brief Get the first animation associated with the clump
CAnimBlendAssociation *RpAnimBlendClumpGetFirstAssociation(RpClump *clump);

//! @addr 0x4D6A70
CAnimBlendAssociation *RpAnimBlendClumpGetFirstAssociation(RpClump *clump, uint32 flags);

//! @addr 0x4D6910
//! @brief Get the 1st (and optionally 2nd) non-partial animations with the highest blend value
CAnimBlendAssociation *RpAnimBlendClumpGetMainAssociation(RpClump *clump, CAnimBlendAssociation **pp2ndAnim, float *pBlendVal2nd);

//! @addr 0x4D6A30
//! @brief Get the `n`th non-partial animation
CAnimBlendAssociation *RpAnimBlendClumpGetMainAssociation_N(RpClump *clump, uint32 n);

//! @brief 0x4D69A0
CAnimBlendAssociation *RpAnimBlendClumpGetMainPartialAssociation(RpClump *clump);

//! @brief 0x4D69F0
//! @brief Get the `n`th partial animation
CAnimBlendAssociation *RpAnimBlendClumpGetMainPartialAssociation_N(RpClump *clump, int32 n);

//! @brief 0x4D6B60
uint32 RpAnimBlendClumpGetNumAssociations(RpClump *clump);

//! @brief 0x4D6BB0
uint32 RpAnimBlendClumpGetNumNonPartialAssociations(RpClump *clump);

//! @brief 0x4D6B80
uint32 RpAnimBlendClumpGetNumPartialAssociations(RpClump *clump);

//! @addr 0x4D6720
//! @brief Initialize clump data for RpAnimBlend
void RpAnimBlendClumpInit(RpClump *clump);

//! @addr 0x4D6760
//! @brief Check if the RpAnimBlend data of the clump is initialized
bool RpAnimBlendClumpIsInitialized(RpClump *clump);

//! @addr 0x4D6B00
//! @brief Pause all animations of a clump
void RpAnimBlendClumpPauseAllAnimations(RpClump *clump);

//! @addr 0x4D6B30
//! @brief Un-pause all animations of a clump
void RpAnimBlendClumpUnPauseAllAnimations(RpClump *clump);

//! @addr 0x4D6C00
//! @brief Remove all animations of a clump
void RpAnimBlendClumpRemoveAllAssociations(RpClump *clump);

//! @addr 0x4D6820
//! @param flags Flags to check for. If any of these flags is set the animation will be removed. Ignored if `0`.
void RpAnimBlendClumpRemoveAssociations(RpClump *clump, uint32 flags = 0);

//! @addr 0x4D67E0
//! @brief Set the blend delta to the specified value for all animations of the clump
void RpAnimBlendClumpSetBlendDeltas(RpClump *clump, uint32 flags, float blendDelta);

//! @addr 0x4D34F0
//! @brief Update all animations associated with the clump
void RpAnimBlendClumpUpdateAnimations(RpClump *clump, float step, bool onScreen);

/*!
 * @addr 0x4D60E0
 * @brief R* hacking
 * @details In III and VC R* wrote their own interpolated animation data into a dummy
 * @details RtAnimAnimation and practically didn't use the RtAnimInterpolator.
 * @details In SA they went a step further and now write into the interpolation frames of
 * @details the RtAnimInterpolator instead, almost completely avoiding RtAnimAnimation.
 * @param hierarchy The animation hierarchy
 * @return The animation
 */
RpHAnimAnimation *RpAnimBlendCreateAnimationForHierarchy(RpHAnimHierarchy *hierarchy);

//! @addr 0x4D5EF0
//! @brief Get the name of a (bone?) frame
const char *RpAnimBlendFrameGetName(RwFrame *frame);

//! @addr 0x4D5F00
//! @brief Set the name of a (bone?) frame
void RpAnimBlendFrameSetName(RwFrame *frame, const char *name);

//! @addr 0x4D6AB0
//! @brief Get the next animation in the linked list
CAnimBlendAssociation *RpAnimBlendGetNextAssociation(CAnimBlendAssociation *association);

//! @addr 0x4D6AD0
//! @brief Get the next animation in the linked list
CAnimBlendAssociation *RpAnimBlendGetNextAssociation(CAnimBlendAssociation *association, uint32 flags);
