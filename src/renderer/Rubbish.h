#ifndef __GTA_RUBBISH_H__
#define __GTA_RUBBISH_H__

// The animated leaves/newspapers VC scattered over the map are gone: there is no
// sheet list, no update, no render pass and no StirUp. The class survives only
// because the plane banner's tail still shares the newspaper artwork; it loads
// and frees that one texture. See Rubbish.cpp.
class CRubbish
{
public:
	static void Init(void);
	static void Shutdown(void);
};

extern RwTexture *gpPlaneBannerTexture;

#endif // __GTA_RUBBISH_H__
