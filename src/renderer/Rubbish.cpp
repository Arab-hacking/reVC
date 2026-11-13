#include "common.h"
#include "main.h"

#include "TxdStore.h"
#include "Rubbish.h"

// VC (and the client too) scatters animated leaf and newspaper sheets over the
// map: they lie on the ground, flap in the wind and get kicked into the air when
// a car drives over them (CRubbish::StirUp, called from CAutomobile and CBike).
// The port does not want that, so the whole sheet system is gone — no texture
// reads, no per-frame update, no render pass, and no StirUp call.
//
// What is left is the one thing that shared its artwork: the trailing banner of
// the plane banner (CPlaneBanner in Fluff.cpp) is drawn with the newspaper
// texture. It reads that texture itself now, here, so the renderer keeps working
// without dragging the sheet system back in.
RwTexture *gpPlaneBannerTexture;

void
CRubbish::Init(void)
{
	CTxdStore::PushCurrentTxd();
	int slot = CTxdStore::FindTxdSlot("particle");
	CTxdStore::SetCurrentTxd(slot);
	gpPlaneBannerTexture = RwTextureRead("newspaper01_64", nil);
	CTxdStore::PopCurrentTxd();
}

void
CRubbish::Shutdown(void)
{
	RwTextureDestroy(gpPlaneBannerTexture);
	gpPlaneBannerTexture = nil;
}
