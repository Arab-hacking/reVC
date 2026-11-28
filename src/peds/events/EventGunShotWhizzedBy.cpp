#include "common.h"

#include "EventGunShotWhizzedBy.h"

#include "Ped.h"

// SA CEventGunShotWhizzedBy. AffectsPed projects the ped onto the
// shot's line and checks the closest distance; the SA reference
// transcribes the projection through a component-wise vector multiply
// whose squared length is not the projection length, so the intended
// dot-product form is used here.

// 0x4B1FD0
CEventGunShotWhizzedBy::CEventGunShotWhizzedBy(CEntity *entity, const CVector &startPoint, const CVector &endPoint, bool bHasNoSound)
: CEventGunShot(entity, startPoint, endPoint, bHasNoSound)
{
}

// 0x4B5120
bool
CEventGunShotWhizzedBy::AffectsPed(CPed *ped)
{
	if(!ped->IsPlayer() && ped->m_fHealth > 0.0f){ // ped->IsAlive()
		CVector direction = m_endPoint - m_startPoint;
		direction.Normalise();
		CVector distance = ped->GetPosition() - m_startPoint;
		float projection = DotProduct(distance, direction);
		if(projection > 0.0f){
			CVector closestPoint = direction * projection + m_startPoint;
			return 2.0f * 2.0f > (ped->GetPosition() - closestPoint).MagnitudeSqr();
		}
	}
	return false;
}
