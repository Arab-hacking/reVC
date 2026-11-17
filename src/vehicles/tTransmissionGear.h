#ifndef __GTA_TTRANSMISSIONGEAR_H__
#define __GTA_TTRANSMISSIONGEAR_H__

// One gear of a cTransmission. Gear 0 is reverse, 1..m_nNumberOfGears are forward.
struct tTransmissionGear
{
	float m_MaxVelocity;			// top speed this gear allows
	float m_ChangeUpVelocity;		// speed at which the box shifts up
	float m_ChangeDownVelocity;	// speed at which the box shifts down
};

#endif // __GTA_TTRANSMISSIONGEAR_H__
