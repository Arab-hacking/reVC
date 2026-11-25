#ifndef __GTA_FLUFF_H__
#define __GTA_FLUFF_H__
#include "common.h"
#include "Vector.h"
#include "Object.h"

class CEscalator
{
	CVector m_pos0;
	CVector m_pos1;
	CVector m_pos2;
	CVector m_pos3;
	CMatrix m_matrix;
	bool m_bIsActive;
	bool m_bIsMovingDown;
	int32 m_stepsCount;
	float m_lowerEnd; 
	float m_upperEnd; 
	CVector m_midPoint;
	float m_radius;
	CObject *m_pSteps[24];
public:
	CEscalator();
	void Update(void);
	void SwitchOff(void);
	void AddThisOne(CVector pos0, CVector pos1, CVector pos2, CVector pos3, bool b_isMovingDown);
	bool IsActive() const { return m_bIsActive; };
	const CVector& GetPosition() const { return m_midPoint; };
};

class CEscalators
{
	static CEscalator aEscalators[NUM_ESCALATORS];
public:
	static int32 NumEscalators;
	static void Init(void);
	static void Update(void);
	static void Shutdown(void);
	static const CEscalator& GetEscalator(int ind) { return aEscalators[ind]; };
};

class CSmokeTrail {
	CVector m_pos[16];
	float m_opacity[16];
	int m_time[16];
	char m_unused[536];
	int m_seed;
public:
	void Render(void);
	void RegisterPoint(CVector position, float a);
	void Init(int num);
};

class CSmokeTrails {
	static CSmokeTrail aSmoke[3];
public:
	static bool CigOn;
	static void Update(void);
	static void Render(void);
	static void Init(void);
};

#endif // __GTA_FLUFF_H__
