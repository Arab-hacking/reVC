#include "common.h"
#include "AnimManager.h"
#include "AnimBlendAssociation.h"
#include "AnimBlendClumpData.h"
#include "RpAnimBlend.h"
#include "RwHelper.h"
#include "fixtures.h"
#include "CustomModels.h"
#include "IfpReader.h"
#include <unistd.h>
#include <sys/stat.h>
#include <cstdarg>

void re3_debug(const char *, ...) {}
void re3_assert(const char *expr,const char *file,unsigned line,const char *) {
	fprintf(stderr,"%s:%u: %s\n",file,line,expr); abort();
}
void DebugLogPrintf(const char *, ...) {}

static void load(Bytes b,const char *bank,bool compressed) {
	RwMemory mem={b.data(),(uint32)b.size()};
	RwStream *stream=RwStreamOpen(rwSTREAMMEMORY,rwSTREAMREAD,&mem);
	CAnimManager::LoadAnimFile(stream,compressed,nil,bank); RwStreamClose(stream,nil);
	auto block=CAnimManager::GetAnimationBlock(bank); assert(block && block->isLoaded);
	auto h=CAnimManager::GetAnimation("walk_civi",block); assert(h);
#ifdef ANIM_COMPRESSION
	if(h->compressed) h->Uncompress();
#endif
	assert(h->totalLength==1.0f);
	for(int i=0;i<h->numSequences;i++) {
		auto &seq=h->sequences[i]; auto k=seq.GetKeyFrame(1);
		assert(k->deltaTime==1.0f);
		assert(k->rotation.x==(b[3]=='K'?-0.5f:0.5f));
		if(seq.HasTranslation()) {
			auto t=(KeyFrameTrans*)k;
			assert(t->translation.x==2 && t->translation.y==3 && t->translation.z==4);
		}
	}
}
static void customFolder() {
	char cwd[4096]; assert(getcwd(cwd,sizeof(cwd)));
	char dir[]="build/sa-tests/custom-XXXXXX"; assert(mkdtemp(dir)); assert(chdir(dir)==0);
	assert(mkdir("Custom",0700)==0); unsetenv("REVC_CUSTOM_DIR");
	auto b=compact(true,true);
	FILE *f=fopen("Custom/PED.IFP","wb"); assert(f);
	assert(fwrite(b.data(),1,b.size(),f)==b.size()); fclose(f);
	Bytes loaded; ifp::Result r;
	assert(CCustomModels::LoadAnimFileFromCustom("ANIM\\PED.IFP",loaded));
	assert(ifp::Inspect(loaded.data(),loaded.size(),r) && !r.mobile && r.sa);
	load(loaded,"test_mobile",false);
	b=anpk(); f=fopen("Custom/sa_ped.ifp","wb"); assert(f);
	assert(fwrite(b.data(),1,b.size(),f)==b.size()); fclose(f);
	CCustomModels::Shutdown(); CAnimManager::LoadSAAnimFileFromCustom("ANIM\\PED.IFP");
	auto block=CAnimManager::GetAnimationBlock("sa_ped"); assert(block && block->isLoaded);
	assert(CAnimManager::GetAnimation("walk_civi",block)->numSequences==1);
	assert(CAnimManager::GetAnimationBlock("ped")==nil);
	CCustomModels::Shutdown(); setenv("REVC_CUSTOM_DIR","missing",1);
	assert(!CCustomModels::LoadAnimFileFromCustom("ped.ifp",loaded));
	CCustomModels::Shutdown(); unsetenv("REVC_CUSTOM_DIR");
	assert(unlink("Custom/PED.IFP")==0); assert(unlink("Custom/sa_ped.ifp")==0);
	assert(rmdir("Custom")==0); assert(chdir(cwd)==0); assert(rmdir(dir)==0);
	puts("Custom: PASS (capitalized directory, mobile normalization, explicit SA ANPK, env override)");
}
int main() {
	rw::Engine::init(nil); rw::registerHAnimPlugin(); rw::registerSkinPlugin();
	RpAnimBlendPluginAttach(); rw::Engine::open(nil); rw::Engine::start();
	load(compact(false),"test_anp2",false); load(compact(),"test_anp3",true);
	load(anpk(),"test_anpk",false); // override must not overwrite the native ped bank
	assert(CAnimManager::GetAnimationBlock("ped")==nil);

	customFolder();
	RpClump *clump=RpClumpCreate(); RwFrame *root=RwFrameCreate(); RpClumpSetFrame(clump,root);
	int ids[]={0,1,2,3,4,5,31,32,33,34,35,21,22,23,24,25,41,42,43,51,52,53,26};
	int flags[23]={};
	auto hier=rw::HAnimHierarchy::create(23,flags,ids,0,36);
	rw::HAnimData::get(root)->hierarchy=hier;
	assert(GetAnimHierarchyFromClump(clump)==hier); // hierarchy on root, not a child
	RpAtomic *atomic=RpAtomicCreate(); RpGeometry *geo=RpGeometryCreate(0,0,0);
	RpAtomicSetGeometry(atomic,geo,0); RpGeometryDestroy(geo); RpClumpAddAtomic(clump,atomic);
	rw::Skin skin={}; rw::Skin::set(atomic->geometry,&skin); rw::Skin::setHierarchy(atomic,hier);
	assert(IsSAAnimationClump(clump)); hier->nodeInfo[22].id=25;
	assert(!IsSAAnimationClump(clump)); hier->nodeInfo[22].id=26;

	RpAnimBlendAllocateData(clump); auto data=*RPANIMBLENDCLUMPDATA(clump);
	data->SetNumberOfFrames(2); data->frames[0].nodeID=1; data->frames[1].nodeID=0;
	auto h=CAnimManager::GetAnimation("walk_civi",CAnimManager::GetAnimationBlock("test_anp2"));
	{
		CAnimBlendAssociation a; a.Init(clump,h);
		assert(a.nodes[0].sequence==&h->sequences[1]);
		data->SetNumberOfFrames(3); data->frames[0].nodeID=26;
		data->frames[1].nodeID=0; data->frames[2].nodeID=1;
		a.Rebind(clump);
		assert(a.numNodes==3 && a.nodes[0].sequence==&h->sequences[2] &&
		       a.nodes[1].sequence==&h->sequences[0] && a.nodes[2].sequence==&h->sequences[1]);
	}
	rw::Skin::set(atomic->geometry,nil); RpClumpDestroy(clump);
	puts("Runtime: PASS (real IFP loader, keyframe values, root hierarchy, VC/SA detection, bone rebinding)");
}

// Unused game services retained by linker through streaming/file branches.
#include "Streaming.h"
#include "FileMgr.h"
CStreamingInfo CStreaming::ms_aInfoForModel[NUMSTREAMINFO];
void CStreaming::RemoveModel(int32) { abort(); }
bool CFileMgr::ResolveBundledGameFile(const char *, char *, size_t) { return false; }
char *casepath(const char *, bool) { abort(); }
