#include "common.h"

#include "AudioScriptObject.h"
#include "Pools.h"
#include "DMAudio.h"

cAudioScriptObject::cAudioScriptObject()
{
	Reset();
};

cAudioScriptObject::~cAudioScriptObject()
{
	Reset();
};

void
cAudioScriptObject::Reset()
{
	AudioId = SCRIPT_SOUND_INVALID;
	Posn = CVector(0.0f, 0.0f, 0.0f);
	AudioEntity = AEHANDLE_NONE;
}

void *
cAudioScriptObject::operator new(size_t sz) throw()
{
	return CPools::GetAudioScriptObjectPool()->New();
}

void *
cAudioScriptObject::operator new(size_t sz, int handle) throw()
{
	return CPools::GetAudioScriptObjectPool()->New(handle);
}

void
cAudioScriptObject::operator delete(void *p, size_t sz) throw()
{
	CPools::GetAudioScriptObjectPool()->Delete((cAudioScriptObject *)p);
}

void
cAudioScriptObject::operator delete(void *p, int handle) throw()
{
	CPools::GetAudioScriptObjectPool()->Delete((cAudioScriptObject *)p);
}



void
PlayOneShotScriptObject(uint8 id, CVector const &pos)
{
	if (!DMAudio.IsAudioInitialised()) return;

	cAudioScriptObject *audioScriptObject = new cAudioScriptObject();
	audioScriptObject->Posn = pos;
	audioScriptObject->AudioId = id;
	audioScriptObject->AudioEntity = AEHANDLE_NONE;
	DMAudio.CreateOneShotScriptObject(audioScriptObject);
}
