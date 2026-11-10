#include "common.h"
#include "ColBox.h"

void
CColBox::Set(const CVector &min, const CVector &max, uint8 material, uint8 light, uint8 surface)
{
	this->min = min;
	this->max = max;
	this->material = material;
	this->light = light;
	this->surface = surface;
}

void
CColBox::Set(const CVector &min, const CVector &max, uint8 surf, uint8 piece)
{
	this->min = min;
	this->max = max;
	this->surface = surf;
	this->material = piece;
}

CColBox&
CColBox::operator=(const CColBox& other)
{
	min = other.min;
	max = other.max;
	material = other.material;
	light = other.light;
	surface = other.surface;
	return *this;
}
