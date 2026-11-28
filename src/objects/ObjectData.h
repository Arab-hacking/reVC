#ifndef __GTA_OBJECTDATA_H__
#define __GTA_OBJECTDATA_H__

class CObject;

class CObjectInfo
{
public:
	float m_fMass;
	float m_fTurnMass;
	float m_fAirResistance;
	float m_fElasticity;
	float m_fBuoyancy;
	float m_fUprootLimit;
	float m_fCollisionDamageMultiplier;
	uint8 m_nCollisionDamageEffect;
	uint8 m_nSpecialCollisionResponseCases;
	bool m_bCameraToAvoidThisObject;
};
VALIDATE_SIZE(CObjectInfo, 0x20);

class CObjectData
{
	static CObjectInfo ms_aObjectInfo[NUMOBJECTINFO];
	// The table itself is parsed from memory: the archive copy is larger than
	// the shared work buffer, so both sources feed this one routine.
	static void ParseObjectData(const char *data);
public:
	static void Initialise(const char *filename);
	static void SetObjectData(int32 modelId, CObject &object);
};

#endif // __GTA_OBJECTDATA_H__
