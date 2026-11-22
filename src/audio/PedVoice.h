#ifndef __GTA_PEDVOICE_H__
#define __GTA_PEDVOICE_H__

// Which speech set a ped draws its voice from (CAEPedSpeechAudioEntity's
// eAudioPedType). The ped's ide row names one of these, and the two voice
// fields name entries inside that set.
enum eAudioPedType
{
	PED_TYPE_UNK = -1,
	PED_TYPE_GEN = 0,
	PED_TYPE_EMG = 1,
	PED_TYPE_PLAYER = 2,
	PED_TYPE_GANG = 3,
	PED_TYPE_GFD = 4,
	PED_TYPE_SPC = 5,

	PED_TYPE_NUM
};

// A voice is identified by its index inside its set; -1 is "none".
enum { VOICE_UNK = -1 };

class CPedVoice
{
public:
	// CAEPedSpeechAudioEntity::GetAudioPedType, 0x4E3C60.
	static eAudioPedType GetAudioPedType(const char *name);
	// CAEPedSpeechAudioEntity::GetVoice, 0x4E3CD0.
	static int16 GetVoice(const char *name, eAudioPedType type);
};

#endif // __GTA_PEDVOICE_H__
