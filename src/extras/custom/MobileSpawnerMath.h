#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace mobilespawner {

enum PanelMode {
	PANEL_NONE = 0,
	PANEL_SKIN = 1,
	PANEL_VEHICLE = 2
};

enum PanelHit {
	PHIT_NONE = 0,
	PHIT_OUTSIDE,
	PHIT_PREV,
	PHIT_STEP_MINUS10,
	PHIT_STEP_PLUS10,
	PHIT_NEXT,
	PHIT_CLOSE,
	PHIT_ROW_0,
	PHIT_ROW_1,
	PHIT_ROW_2,
	PHIT_ROW_3,
	PHIT_ROW_4,
	PHIT_ROW_5
};

struct CatalogItem {
	int32_t modelId;
	bool isCustom;
	bool isSecondaryTag; // SA skeleton for skins; Bike for vehicles
	char name[28];
};

// Sort custom slots (>= firstCustomSlot, e.g. 4900) BEFORE stock VC slots,
// with MI_PLAYER (0) kept first among stock skins so returning to Tommy is 1 tap.
inline void
SortCatalogItems(std::vector<CatalogItem> &items, bool keepZeroFirst = false)
{
	std::sort(items.begin(), items.end(), [keepZeroFirst](const CatalogItem &a, const CatalogItem &b) {
		if (keepZeroFirst) {
			if (a.modelId == 0 && b.modelId != 0) return true;
			if (b.modelId == 0 && a.modelId != 0) return false;
		}
		if (a.isCustom != b.isCustom)
			return a.isCustom > b.isCustom;
		return a.modelId < b.modelId;
	});
}

// Exclude non-drivable ambient/RC prop vehicle IDs in Vice City:
// MI_CHOPPER(165), MI_RCBANDIT(171), MI_AIRTRAIN(180), MI_DEADDODO(181),
// MI_RCBARON(194), MI_RCRAIDER(195), MI_RCGOBLIN(231).
inline bool
IsExcludedStockVehicleId(int32_t id)
{
	return id == 165 || id == 171 || id == 180 || id == 181 ||
	       id == 194 || id == 195 || id == 231;
}

// Compute spawn position in front of the player/camera without relying on CPathFind.
inline void
ComputeVehicleSpawnTransform(float baseX, float baseY, float baseZ,
                             float fwdX, float fwdY, float forwardDist,
                             float *outX, float *outY, float *outZ,
                             float *outRightX, float *outRightY)
{
	float len2 = fwdX * fwdX + fwdY * fwdY;
	float nx = 0.0f, ny = 1.0f;
	if (len2 > 1.0e-5f) {
		float inv = 1.0f / std::sqrt(len2);
		nx = fwdX * inv;
		ny = fwdY * inv;
	}
	if (outX) *outX = baseX + nx * forwardDist;
	if (outY) *outY = baseY + ny * forwardDist;
	if (outZ) *outZ = baseZ;
	// Park vehicle angled slightly / facing along player's forward vector:
	// GTA VC matrix: up = forward (nx, ny), right = (ny, -nx)
	if (outRightX) *outRightX = ny;
	if (outRightY) *outRightY = -nx;
}

struct PickerLayout {
	float left, top, right, bottom;
	float headerBottom;
	float rowTop, rowHeight;
	int visibleRows;

	float BtnCenterX(int idx) const {
		float w = (right - left) / 5.0f;
		return left + w * ((float)idx + 0.5f);
	}
	float BtnCenterY() const {
		return 0.5f * (top + headerBottom);
	}
};

inline PickerLayout
BuildPickerLayout(float screenW, float screenH, float anchorX, float topBarBottomY, float dp1)
{
	const float cardW = std::min(dp1 * 268.0f, screenW * 0.52f);
	const float headerH = dp1 * 42.0f;
	const float rowH = dp1 * 34.0f;
	const int visRows = 6;
	const float cardH = headerH + rowH * (float)visRows + dp1 * 8.0f;

	PickerLayout L;
	L.left = std::max(dp1 * 12.0f, std::min(anchorX - cardW * 0.35f, screenW - cardW - dp1 * 12.0f));
	L.top = std::max(dp1 * 8.0f, std::min(topBarBottomY + dp1 * 6.0f, screenH - cardH - dp1 * 8.0f));
	L.right = L.left + cardW;
	L.bottom = L.top + cardH;
	L.headerBottom = L.top + headerH;
	L.rowTop = L.headerBottom + dp1 * 4.0f;
	L.rowHeight = rowH;
	L.visibleRows = visRows;
	return L;
}

inline PanelHit
HitTestPicker(const PickerLayout &L, float x, float y)
{
	if (x < L.left || x > L.right || y < L.top || y > L.bottom)
		return PHIT_OUTSIDE;
	if (y <= L.headerBottom) {
		int col = (int)((x - L.left) * 5.0f / std::max(L.right - L.left, 1.0f));
		if (col < 0) col = 0;
		if (col > 4) col = 4;
		switch (col) {
		case 0: return PHIT_PREV;
		case 1: return PHIT_STEP_MINUS10;
		case 2: return PHIT_STEP_PLUS10;
		case 3: return PHIT_NEXT;
		default: return PHIT_CLOSE;
		}
	}
	if (y >= L.rowTop) {
		int row = (int)((y - L.rowTop) / std::max(L.rowHeight, 1.0f));
		if (row >= 0 && row < L.visibleRows)
			return (PanelHit)((int)PHIT_ROW_0 + row);
	}
	return PHIT_NONE;
}

inline int
WrapIndex(int idx, int count)
{
	if (count <= 0) return 0;
	int r = idx % count;
	return r < 0 ? r + count : r;
}

} // namespace mobilespawner
