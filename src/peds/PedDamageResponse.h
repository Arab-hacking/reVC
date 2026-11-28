#pragma once

// SA CPedDamageResponse: the outcome of running damage through
// CPedDamageResponseCalculator - how much health/armour it costs and
// whether it kills. Computed once (m_bDamageCalculated), then shared by
// the damage event, the death logic and the response tasks.

class CPedDamageResponse
{
public:
	float m_fDamageHealth;
	float m_fDamageArmor;
	bool m_bHealthZero;
	bool m_bForceDeath;
	bool m_bDamageCalculated;
	bool m_bCheckIfAffectsPed;

	CPedDamageResponse()
	{
		m_fDamageHealth = 0.0f;
		m_fDamageArmor = 0.0f;
		m_bHealthZero = false;
		m_bForceDeath = false;
		m_bDamageCalculated = false;
		m_bCheckIfAffectsPed = false;
	}

	void SetDamageAsCalculated() { m_bDamageCalculated = true; }
	float GetTotalDamage() const { return m_fDamageArmor + m_fDamageHealth; }
};

VALIDATE_SIZE(CPedDamageResponse, 0xC);
