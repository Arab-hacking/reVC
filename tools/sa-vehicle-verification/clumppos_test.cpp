// clumppos_test.cpp - where does the SA two-part clump reader leave the stream?
//
// reVC reads SA style models in two parts (frames+geometries, then atomics)
// with RpClumpGtaStreamRead1/2. Anything a model stores *after* the atomics
// (SA keeps its collision model there) can only be read if the stream is not
// already past it, so this test prints the stream position after every step.
//
// usage: clumppos_test <file.dff>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "ModelInfo.h"
#include "VehicleModelInfo.h"
#include "FileLoader.h"
#include "RwHelper.h"
#include "NodeName.h"
#include "VisibilityPlugins.h"
#include "TxdStore.h"
#include "TempColModels.h"
#include "RpAnimBlend.h"
#include <rpmatfx.h>

using namespace rw;

int main(int argc, char **argv)
{
	if(argc < 2){ printf("usage: clumppos_test <file.dff>\n"); return 1; }
	setvbuf(stdout, nil, _IONBF, 0);
	printf("init\n"); Engine::init();
	printf("open\n"); Engine::open(nil);
	printf("start\n"); Engine::start();
	printf("plugins\n");
	RpWorldPluginAttach();
	RpSkinPluginAttach();
	RpHAnimPluginAttach();
	NodeNamePluginAttach();
	CVisibilityPlugins::PluginAttach();
	RpAnimBlendPluginAttach();
	RpMatFXPluginAttach();
	CTxdStore::Initialise();
	CTempColModels::Initialise();
	CModelInfo::Initialise();

	StreamFile sf;
	if(!sf.open(argv[1], "rb")){ printf("cannot open\n"); return 2; }

	// the game: CFileLoader::StartLoadClumpFile finds the clump chunk, then the
	// two part reader runs; the question is where that leaves the stream
	uint32 clumpSize = 0, ver = 0, clumpBody = 0;
	if(!RwStreamFindChunk(&sf, rwID_CLUMP, &clumpSize, &ver)){ printf("no CLUMP chunk\n"); return 2; }
	clumpBody = (uint32)sf.tell();
	printf("clump body starts at %u, size %u (ends at %u)\n", clumpBody, clumpSize, clumpBody + clumpSize);

	RpClumpGtaStreamRead1(&sf);
	printf("after part 1        = %d\n", (int)sf.tell());
	RpClump *clump = RpClumpGtaStreamRead2(&sf);
	printf("after part 2        = %d   (clump %s)\n", (int)sf.tell(), clump ? "read" : "FAILED");

	// and here is where the collision chunk sits
	sf.seek(clumpBody, 0);
	if(RwStreamFindChunk(&sf, rwID_EXTENSION, nil, &ver))
		printf("clump extension body at %u\n", (uint32)sf.tell());
	if(RwStreamFindChunk(&sf, 0x0253F2FA, &ver, nil)){
		printf("collision chunk body at %u  <- part 2 does not stop on a chunk\n"
		       "      boundary (it ends inside the last atomic), so a plain RwStreamFindChunk\n"
		       "      from there cannot find it: the chunk walk from the clump start is needed\n",
		       (uint32)sf.tell());
	}else{
		printf("collision chunk not found\n");
	}
	RpClumpDestroy(clump);
	return 0;
}
