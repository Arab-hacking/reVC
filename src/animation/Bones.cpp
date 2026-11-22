#include "common.h"
#include "PedModelInfo.h"
#include "Bones.h"

// CPedModelInfo::ConvertPedNode2BoneTag, 0x4D58A0.
// Every node the model names has a bone; only the whole-body node (slot 0) and
// an out-of-range index have none, and those answer BONE_UNKNOWN.
int
ConvertPedNode2BoneTag(ePedNode node)
{
	switch(node){
	case PED_NODE_UPPER_TORSO:		return BONE_SPINE1;
	case PED_NODE_HEAD:				return BONE_HEAD;
	case PED_NODE_LEFT_ARM:			return BONE_L_UPPER_ARM;
	case PED_NODE_RIGHT_ARM:		return BONE_R_UPPER_ARM;
	case PED_NODE_LEFT_HAND:		return BONE_L_HAND;
	case PED_NODE_RIGHT_HAND:		return BONE_R_HAND;
	case PED_NODE_LEFT_LEG:			return BONE_L_THIGH;
	case PED_NODE_RIGHT_LEG:		return BONE_R_THIGH;
	case PED_NODE_LEFT_FOOT:		return BONE_L_FOOT;
	case PED_NODE_RIGHT_FOOT:		return BONE_R_FOOT;
	case PED_NODE_RIGHT_LOWER_LEG:	return BONE_R_CALF;
	case PED_NODE_LEFT_LOWER_LEG:	return BONE_L_CALF;
	case PED_NODE_LEFT_LOWER_ARM:	return BONE_L_FORE_ARM;
	case PED_NODE_RIGHT_LOWER_ARM:	return BONE_R_FORE_ARM;
	case PED_NODE_LEFT_CLAVICLE:	return BONE_L_CLAVICLE;
	case PED_NODE_RIGHT_CLAVICLE:	return BONE_R_CLAVICLE;
	case PED_NODE_NECK:				return BONE_NECK;
	case PED_NODE_JAW:				return BONE_JAW;
	default:						return BONE_UNKNOWN;
	}
}

// ConvertBoneTag2BoneName, 0x4D56F0. The names are the ones the San Andreas
// skeletons carry; a tag with no named bone answers nil.
const char*
ConvertBoneTag2BoneName(int tag)
{
	switch(tag){
	case BONE_R_BREAST:		return "R Breast";
	case BONE_L_BREAST:		return "L Breast";
	case BONE_BELLY:		return "Belly";
	case BONE_ROOT:			return "Root";
	case BONE_PELVIS:		return "Pelvis";
	case BONE_SPINE:		return "Spine";
	case BONE_SPINE1:		return "Spine1";
	case BONE_NECK:			return "Neck";
	case BONE_HEAD:			return "Head";
	case BONE_L_BROW:		return "L Brow";
	case BONE_R_BROW:		return "R Brow";
	case BONE_JAW:			return "Jaw";
	case BONE_R_CLAVICLE:	return "Bip01 R Clavicle";
	case BONE_R_UPPER_ARM:	return "R UpperArm";
	case BONE_R_FORE_ARM:	return "R Forearm";
	case BONE_R_HAND:		return "R Hand";
	case BONE_R_FINGER:		return "R Fingers";
	case BONE_R_FINGER_01:	return "R Finger01";
	case BONE_L_CLAVICLE:	return "Bip01 L Clavicle";
	case BONE_L_UPPER_ARM:	return "L UpperArm";
	case BONE_L_FORE_ARM:	return "L Forearm";
	case BONE_L_HAND:		return "L Hand";
	case BONE_L_FINGER:		return "L Fingers";
	case BONE_L_FINGER_01:	return "L Finger01";
	case BONE_L_THIGH:		return "L Thigh";
	case BONE_L_CALF:		return "L Calf";
	case BONE_L_FOOT:		return "L Foot";
	case BONE_L_TOE_0:		return "L Toe0";
	case BONE_R_THIGH:		return "R Thigh";
	case BONE_R_CALF:		return "R Calf";
	case BONE_R_FOOT:		return "R Foot";
	case BONE_R_TOE_0:		return "R Toe0";
	}
	return nil;
}
