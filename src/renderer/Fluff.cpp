#include "common.h"
#include "Pools.h"
#include "main.h"

#include "RenderBuffer.h"
#include "Entity.h"
#include "Fluff.h"
#include "Game.h"
#include "Camera.h"
#include "Sprite.h"
#include "Coronas.h"
#include "PointLights.h"
#include "TxdStore.h"
#include "Timecycle.h"
#include "General.h"
#include "Timer.h"
#include "Clock.h"
#include "Weather.h"
#include "Stats.h"
#include "maths.h"
#include "Frontend.h"
#include "PlayerPed.h"
#include "Bones.h"
#include "World.h"
#include "Coronas.h"
#include "SaveBuf.h"
#ifdef CUSTOM_MODELS
#include "CustomModels.h"
#endif

bool CSmokeTrails::CigOn = false;
CSmokeTrail CSmokeTrails::aSmoke[3];

RwImVertexIndex SmokeTrailIndices[32] = { 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16 };

float RandomSmoke[16] = { 10.0f, 5.0f, -1.0f, -9.0f, -7.0f, -1.0f, 0.0f, 3.0f, 6.0f, 7.0f, 4.0f, 2.0f,
5.0f, 7.0f };

uint8 ScrollCharSet[59][5] = {
	{ 0x00, 0x00, 0x00, 0x00, 0x00 }, // ' '
	{ 0x00, 0x00, 0x1D, 0x00, 0x00 }, // '!'
	{ 0x00, 0x00, 0x00, 0x00, 0x00 }, // '"'
	{ 0x0A, 0x1F, 0x0A, 0x1F, 0x0A }, // '#'
	{ 0x00, 0x09, 0x1F, 0x12, 0x00 }, // '$'
	{ 0x18, 0x18, 0x00, 0x03, 0x03 }, // '%'
	{ 0x00, 0x00, 0x00, 0x00, 0x00 }, // '&'
	{ 0x00, 0x00, 0x00, 0x00, 0x00 }, // '''
	{ 0x01, 0x02, 0x04, 0x08, 0x10 }, // '('
	{ 0x00, 0x00, 0x18, 0x00, 0x00 }, // ')'
	{ 0x15, 0x04, 0x1F, 0x04, 0x15 }, // '*'
	{ 0x00, 0x04, 0x0E, 0x04, 0x00 }, // '+'
	{ 0x00, 0x00, 0x03, 0x00, 0x00 }, // ','
	{ 0x00, 0x04, 0x04, 0x04, 0x00 }, // '-'
	{ 0x00, 0x00, 0x01, 0x00, 0x00 }, // '.'
	{ 0x00, 0x00, 0x00, 0x00, 0x00 }, // '/'
	{ 0x0E, 0x11, 0x11, 0x11, 0x0E }, // '0'
	{ 0x01, 0x09, 0x1F, 0x01, 0x01 }, // '1'
	{ 0x03, 0x15, 0x15, 0x15, 0x09 }, // '2'
	{ 0x11, 0x11, 0x15, 0x15, 0x0A }, // '3'
	{ 0x02, 0x06, 0x0A, 0x1F, 0x02 }, // '4'
	{ 0x1D, 0x15, 0x15, 0x15, 0x12 }, // '5'
	{ 0x0E, 0x15, 0x15, 0x15, 0x12 }, // '6'
	{ 0x18, 0x10, 0x13, 0x14, 0x18 }, // '7'
	{ 0x0A, 0x15, 0x15, 0x15, 0x0A }, // '8'
	{ 0x08, 0x15, 0x15, 0x15, 0x0E }, // '9'
	{ 0x00, 0x00, 0x0A, 0x00, 0x00 }, // ':'
	{ 0x18, 0x18, 0x00, 0x03, 0x03 }, // ';'
	{ 0x04, 0x08, 0x1F, 0x08, 0x04 }, // '<'
	{ 0x00, 0x0A, 0x0A, 0x0A, 0x00 }, // '='
	{ 0x04, 0x02, 0x1F, 0x02, 0x04 }, // '>'
	{ 0x10, 0x10, 0x15, 0x14, 0x1D }, // '?'
	{ 0x00, 0x1C, 0x14, 0x1C, 0x00 }, // '@'
	{ 0x0F, 0x12, 0x12, 0x12, 0x0F }, // 'A'
	{ 0x1F, 0x15, 0x15, 0x15, 0x0A }, // 'B'
	{ 0x0E, 0x11, 0x11, 0x11, 0x0A }, // 'C'
	{ 0x1F, 0x11, 0x11, 0x11, 0x0E }, // 'D'
	{ 0x1F, 0x15, 0x15, 0x11, 0x11 }, // 'E'
	{ 0x1F, 0x14, 0x14, 0x10, 0x10 }, // 'F'
	{ 0x0E, 0x11, 0x15, 0x15, 0x06 }, // 'G'
	{ 0x1F, 0x04, 0x04, 0x04, 0x1F }, // 'H'
	{ 0x11, 0x11, 0x1F, 0x11, 0x11 }, // 'I'
	{ 0x02, 0x01, 0x01, 0x01, 0x1E }, // 'J'
	{ 0x1F, 0x04, 0x0C, 0x12, 0x01 }, // 'K'
	{ 0x1F, 0x01, 0x01, 0x01, 0x01 }, // 'L'
	{ 0x1F, 0x08, 0x06, 0x08, 0x1F }, // 'M'
	{ 0x1F, 0x08, 0x04, 0x02, 0x1F }, // 'N'
	{ 0x0E, 0x11, 0x11, 0x11, 0x0E }, // 'O'
	{ 0x1F, 0x12, 0x12, 0x12, 0x0C }, // 'P'
	{ 0x0C, 0x12, 0x12, 0x13, 0x0D }, // 'Q'
	{ 0x1F, 0x14, 0x14, 0x16, 0x09 }, // 'R'
	{ 0x09, 0x15, 0x15, 0x15, 0x02 }, // 'S'
	{ 0x10, 0x10, 0x1F, 0x10, 0x10 }, // 'T'
	{ 0x1E, 0x01, 0x01, 0x01, 0x1E }, // 'U'
	{ 0x1C, 0x02, 0x01, 0x02, 0x1C }, // 'V'
	{ 0x1E, 0x01, 0x06, 0x01, 0x1E }, // 'W'
	{ 0x11, 0x0A, 0x04, 0x0A, 0x11 }, // 'X'
	{ 0x18, 0x04, 0x03, 0x04, 0x18 }, // 'Y'
	{ 0x11, 0x13, 0x15, 0x19, 0x11 }  // 'Z'
};

void
CSmokeTrail::RegisterPoint(CVector regPosition, float opacity) {
	bool bAddedNewPoint = false;

	if (m_time[0] && CTimer::GetTimeInMilliseconds() - m_time[0] > 150) {
		bAddedNewPoint = true;
		for (int32 i = 15; i > 0; i--) {
			m_pos[i] = m_pos[i - 1];
			m_time[i] = m_time[i - 1];
			m_opacity[i] = m_opacity[i - 1];
		}
		++m_seed;
	}
	m_pos[0] = regPosition;

	if (bAddedNewPoint || !m_time[0]) {
		m_time[0] = CTimer::GetTimeInMilliseconds();
		float density = 0.1f / (m_pos[1] - m_pos[2]).Magnitude();
		m_opacity[1] = opacity * Min(density, 1.0f);
	}
	m_opacity[0] = 0.0f;
}

void
CSmokeTrail::Init(int num) {
	for (int32 i = 0; i < 16; i++)
		m_time[i] = 0;
	m_seed = num * 2;
}

void
CSmokeTrails::Init(void) {
	for (int32 i = 0; i < 3; i++)
		aSmoke[i].Init(i);
}

void
CSmokeTrails::Render(void) {
	for (int32 i = 0; i < 3; i++)
		aSmoke[i].Render();
}

void
CSmokeTrail::Render(void) {
	int numVerts = 0;

	if (TheCamera.IsSphereVisible(m_pos[0], 10.0f)) {
		for (int32 i = 0; i < 16; i++) {
			int timeSinceSpawned = CTimer::GetTimeInMilliseconds() - m_time[i];

			if (timeSinceSpawned > 2250)
				m_time[i] = 0;

			if (m_time[i]) {
				int alpha = (1.0f - timeSinceSpawned / 2250.0f) * 110.0f * m_opacity[i];
				float offset = timeSinceSpawned * CWeather::Wind * 0.0001f;
				float posX = (m_pos[i].x + timeSinceSpawned * RandomSmoke[(i - m_seed) & 0xF] * 0.00001f) - offset;
				float posY = (m_pos[i].y + timeSinceSpawned * RandomSmoke[(i - m_seed + 5) & 0xF] * 0.00001f) - offset;
				float posZ = m_pos[i].z + timeSinceSpawned * 0.0004f;
				RwIm3DVertexSetRGBA(&TempBufferRenderVertices[i], 200, 200, 200, alpha);
				RwIm3DVertexSetPos(&TempBufferRenderVertices[i], posX, posY, posZ);
				numVerts++;
			}
		}
	}

	if (numVerts > 1) {
		RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
		RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
		RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
		RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nil);

		if (RwIm3DTransform(TempBufferRenderVertices, numVerts, nil, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA)) {
			RwIm3DRenderIndexedPrimitive(rwPRIMTYPEPOLYLINE, SmokeTrailIndices, 2 * (numVerts - 1));
			RwIm3DEnd();
		}
	}
}

void
CSmokeTrails::Update(void) {

	if (!CSmokeTrails::CigOn || TheCamera.Using1stPersonWeaponMode() || !FindPlayerPed() ||
		FindPlayerVehicle() || !FindPlayerPed()->GetClump())
		return;

	RwV3d startPos = { 0.026f, 0.15f, 0.02f };
	RwV3d endPos = { 0.026f, 0.05f, 0.02f };

	RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(FindPlayerPed()->GetClump());
	int32 idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_HEAD));
	RwMatrix *head = &RpHAnimHierarchyGetMatrixArray(hier)[idx];
	RwV3dTransformPoints(&startPos, &startPos, 1, head);
	RwV3dTransformPoints(&endPos, &endPos, 1, head);

	aSmoke[0].RegisterPoint(startPos, 1.0f);
	aSmoke[1].RegisterPoint(startPos, 0.75f);
	aSmoke[2].RegisterPoint(startPos, 0.5f);

	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nil);

	RwIm3DVertexSetRGBA(&TempBufferRenderVertices[0], 255, 255, 255, 255);
	RwIm3DVertexSetPos(&TempBufferRenderVertices[0], startPos.x, startPos.y, startPos.z);
	RwIm3DVertexSetRGBA(&TempBufferRenderVertices[1], 255, 255, 255, 255);
	RwIm3DVertexSetPos(&TempBufferRenderVertices[1], endPos.x, endPos.y, endPos.z);

	if (RwIm3DTransform(TempBufferRenderVertices, 2, nil, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA)) {
		RwIm3DRenderIndexedPrimitive(rwPRIMTYPEPOLYLINE, SmokeTrailIndices, 2);
		RwIm3DEnd();
	}
}

CEscalator CEscalators::aEscalators[NUM_ESCALATORS];
int32 CEscalators::NumEscalators;

CEscalator::CEscalator() {
	m_bIsActive = false;

	for (int i = 0; i < 24; i++) {
		m_pSteps[i] = nil;
	}
}

// This used to be called once per hard-coded native VC site. That list is gone
// with the native map, so nothing registers an escalator any more; the routine
// is kept for its geometry setup.
void
CEscalator::AddThisOne(CVector pos0, CVector pos1, CVector pos2, CVector pos3, bool b_isMovingDown) {
	m_pos0 = pos0;
	m_pos1 = pos1;
	m_pos2 = pos2;
	m_pos3 = pos3;

	float escalatorStepHeight = CModelInfo::GetColModel(MI_ESCALATORSTEP)->boundingBox.max.z;
	m_pos0.z -= escalatorStepHeight;
	m_pos1.z -= escalatorStepHeight;
	m_pos2.z -= escalatorStepHeight;
	m_pos3.z -= escalatorStepHeight;

	float magnitudes[3];
	magnitudes[0] = (m_pos0 - m_pos1).Magnitude();
	magnitudes[1] = (m_pos1 - m_pos2).Magnitude();
	magnitudes[2] = (m_pos2 - m_pos3).Magnitude();

	float length = magnitudes[0] + magnitudes[1] + magnitudes[2];

	m_lowerEnd = magnitudes[0] / length;
	m_upperEnd = (magnitudes[0] + magnitudes[1]) / length;

	m_stepsCount = Min(24.0f, length / 0.6f);

	CVector direction(m_pos0.x - m_pos1.x, m_pos0.y - m_pos1.y, 0.0f);
	direction.Normalise();

	m_matrix.GetUp() = CVector(0.0f, 0.0f, 1.0f);
	m_matrix.GetForward() = CVector(direction.x, direction.y, 0.0f);
	m_matrix.GetRight() = CVector(direction.y, -direction.x, 0.0f);
	m_matrix.GetPosition() = CVector(0.0f, 0.0f, 0.0f);

	m_bIsMovingDown = b_isMovingDown;

	m_midPoint = (m_pos0 + m_pos3) / 2.0f;

	m_radius = (m_pos0 - m_midPoint).Magnitude();
}

void
CEscalator::Update(void) {
	if (!m_bIsActive) {
		if ((TheCamera.GetPosition() - m_midPoint).Magnitude() < 25.0f) {
			if (TheCamera.IsSphereVisible(m_midPoint, m_radius) && (m_stepsCount + 10 < CPools::GetObjectPool()->GetNoOfFreeSpaces())) { 
				m_bIsActive = true;
				for (int i = 0; i < m_stepsCount; i++) {
					m_pSteps[i] = new CObject(MI_ESCALATORSTEP, TRUE);
					if (m_pSteps[i]) {
						m_pSteps[i]->SetPosition(m_pos1);
						CWorld::Add(m_pSteps[i]);
						m_pSteps[i]->ObjectCreatedBy = CONTROLLED_SUB_OBJECT;
					}
				}
			}
		}
	}

	if (m_bIsActive) {
		float time = (CTimer::GetTimeInMilliseconds() % 16384) / 16384.0f;
		for (int i = 0; i < m_stepsCount; i++) {
			if (m_pSteps[i]) {
				float t = i / (float)m_stepsCount + time;

				if (t > 1.0f)
					t -= 1.0f;

				if (m_bIsMovingDown)
					t = 1.0f - t;

				CVector oldPosition = m_pSteps[i]->GetPosition();
				m_pSteps[i]->GetMatrix() = m_matrix;

				CVector newPosition;
				if (t < m_lowerEnd) {
					float ratio = t / m_lowerEnd;
					newPosition = (ratio * m_pos1) + ((1.0f - ratio) * m_pos0);
				}
				else if (t < m_upperEnd) {
					float ratio = (t - m_lowerEnd) / (m_upperEnd - m_lowerEnd);
					newPosition = (ratio * m_pos2) + ((1.0f - ratio) * m_pos1);
				}
				else {
					float ratio = (t - m_upperEnd) / (1.0f - m_upperEnd);
					newPosition = (ratio * m_pos3) + ((1.0f - ratio) * m_pos2);
				}

				m_pSteps[i]->SetPosition(newPosition);
				m_pSteps[i]->m_vecMoveSpeed = (newPosition - oldPosition) / Max(CTimer::GetTimeStep(), 1.0f);
				m_pSteps[i]->GetMatrix().UpdateRW();
				m_pSteps[i]->UpdateRwFrame();
			}
			if ((TheCamera.GetPosition() - m_midPoint).Magnitude() > 28.0f || !TheCamera.IsSphereVisible(m_midPoint, m_radius))
				SwitchOff();
		}
	}
}

bool deletingEscalator;

void
CEscalator::SwitchOff(void) {
	if (m_bIsActive) {
		for (int i = 0; i < m_stepsCount; i++) {
			if (m_pSteps[i]) {
				CWorld::Remove(m_pSteps[i]);
				deletingEscalator = true;
				delete m_pSteps[i];
				m_pSteps[i] = nil;
				deletingEscalator = false;
			}
		}
		m_bIsActive = false;
	}
}

void
CEscalators::Init(void) {
	Shutdown();

	// The 22 escalators below used to be a hard-coded list of native Vice City
	// coordinates, and Esc_step only existed because DATA/MAPS IDEs were read.
	// Neither the island nor its IDE is loaded any more, so the list would place
	// moving steps in empty space over a map the archive does not contain.
	// Nothing is registered; the model-info guards that used to protect this
	// path are gone with the data they tested.
	REVC_LOG_INFO("Skipping native VC escalators (the native map is no longer loaded)");
}

void
CEscalators::Update(void) {

	// An unloaded VC map has no escalators: never update unregistered slots.
	for (int i = 0; i < NumEscalators; i++) {
		aEscalators[i].Update();
	}
}

void
CEscalators::Shutdown(void) {
	for (int i = 0; i < NUM_ESCALATORS; i++) {
		aEscalators[i].SwitchOff();
	}
	NumEscalators = 0;
}

