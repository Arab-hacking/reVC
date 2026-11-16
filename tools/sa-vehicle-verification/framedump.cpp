/* framedump.cpp - print the frame hierarchy of a (SA) DFF with LOCAL and model-space
 * positions, to see what reVC's GetWheelPosn() (which reads the LOCAL matrix!) returns.
 * build: see tools/sa-vehicle-verification/framedump_build.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rw.h>
using namespace rw;

#define VENDOR_ROCKSTAR 0x0253F2
#define MAKECHUNKID(vendor, id) (((vendor & 0xFFFFFF) << 8) | (id & 0xFF))
enum { ID_NODENAME = MAKECHUNKID(VENDOR_ROCKSTAR, 0xFE) };
static int32 gNodeNameOffset = -1;
static void *NodeNameCtor(void *o, int32 off, int32){ if(gNodeNameOffset>0) *(char*)((char*)o+off)='\0'; return o; }
static void *NodeNameDtor(void *o, int32, int32){ return o; }
static void *NodeNameCopy(void *d, void *s, int32 off, int32){ strncpy((char*)((char*)d+off), (char*)((char*)s+off), 23); return nil; }
static Stream *NodeNameRead(Stream *s, int32 len, void *o, int32 off, int32){ if(len>23) len=23; s->read8((char*)o+off, len); ((char*)o)[off+len]='\0'; return s; }
static Stream *NodeNameWrite(Stream *s, int32 len, void *o, int32 off, int32){ s->write8((char*)o+off, len); return s; }
static int32 NodeNameSize(void *o, int32 off, int32){ return (int32)strlen((char*)o+off); }
static const char *FName(Frame *f){ if(gNodeNameOffset<0||f==nil) return "<nil>"; return (char*)f+gNodeNameOffset; }

static int depth;
static Frame *Walk(Frame *f, void *){
	Matrix *local = &f->matrix;
	Matrix *ltm = f->getLTM();
	printf("%*s%-22s local(%7.4f %7.4f %7.4f)  model(%7.4f %7.4f %7.4f)  obj=%s\n",
		depth*2, "", FName(f),
		local->pos.x, local->pos.y, local->pos.z,
		ltm->pos.x, ltm->pos.y, ltm->pos.z,
		!f->objectList.isEmpty() ? "yes" : "no");
	depth++;
	f->forAllChildren(Walk, nil);
	depth--;
	return f;
}

int main(int argc, char **argv){
	if(argc<2){ printf("usage: framedump file.dff [x]\n"); return 1; }
	if(!Engine::init()||!Engine::open(nil)||!Engine::start()){ printf("engine init failed\n"); return 1; }
	gNodeNameOffset = Frame::registerPlugin(24, ID_NODENAME, NodeNameCtor, NodeNameDtor, NodeNameCopy);
	Frame::registerPluginStream(ID_NODENAME, NodeNameRead, NodeNameWrite, NodeNameSize);

	StreamFile sf;
	sf.open(argv[1], "rb");
	if(!findChunk(&sf, ID_CLUMP, nil, nil)){ printf("no CLUMP\n"); return 2; }
	Clump *clump = Clump::streamRead(&sf);
	if(clump==nil){ printf("clump read failed\n"); return 2; }

	printf("frame tree of %s ('local' = what reVC GetWheelPosn reads, 'model' = full transform)\n", argv[1]);
	/* For an SA model the atomics are the second part of the stream; with this
	 * plain read they are already attached, i.e. this is the "fully loaded" state. */
	Walk(clump->getFrame(), nil);
	return 0;
}
