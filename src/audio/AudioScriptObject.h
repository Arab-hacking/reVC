#ifndef __GTA_AUDIOSCRIPTOBJECT_H__
#define __GTA_AUDIOSCRIPTOBJECT_H__

class cAudioScriptObject
{
public:
	int16 AudioId;
	CVector Posn;
	int32 AudioEntity;

	cAudioScriptObject();
	~cAudioScriptObject();

	void Reset(); /// ok

	static void* operator new(size_t) throw();
	static void* operator new(size_t, int) throw();
	static void operator delete(void*, size_t) throw();
	static void operator delete(void*, int) throw();

};

VALIDATE_SIZE(cAudioScriptObject, 20);

extern void PlayOneShotScriptObject(uint8 id, CVector const &pos);

#endif // __GTA_AUDIOSCRIPTOBJECT_H__
