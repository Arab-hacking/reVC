/* geodump.cpp - dump real geometry sizes/positions of an SA DFF via librw */
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

static void MinMax(const V3d *v, int n, float *mn, float *mx){
	mn[0]=mn[1]=mn[2]=1e9f; mx[0]=mx[1]=mx[2]=-1e9f;
	for(int i=0;i<n;i++){
		float c[3]={v[i].x,v[i].y,v[i].z};
		for(int k=0;k<3;k++){ if(c[k]<mn[k])mn[k]=c[k]; if(c[k]>mx[k])mx[k]=c[k]; }
	}
}

int main(int argc, char **argv){
	if(argc<2){ printf("usage: geodump file.dff\n"); return 1; }
	if(!Engine::init()||!Engine::open(nil)||!Engine::start()){ printf("engine init failed\n"); return 1; }
	gNodeNameOffset = Frame::registerPlugin(24, ID_NODENAME, NodeNameCtor, NodeNameDtor, NodeNameCopy);
	Frame::registerPluginStream(ID_NODENAME, NodeNameRead, NodeNameWrite, NodeNameSize);

	StreamFile sf;
	sf.open(argv[1], "rb");
	if(!findChunk(&sf, ID_CLUMP, nil, nil)){ printf("no CLUMP\n"); return 2; }
	Clump *clump = Clump::streamRead(&sf);
	if(clump==nil){ printf("clump read failed\n"); return 2; }

	float modelMin[3]={1e9f,1e9f,1e9f}, modelMax[3]={-1e9f,-1e9f,-1e9f};
	float origModelMin[3]={1e9f,1e9f,1e9f}, origModelMax[3]={-1e9f,-1e9f,-1e9f};

	printf("%-20s %-28s %-28s %-8s\n","frame","frame pos (model space)","vertex bbox (local)","radius");
	FORLIST(lnk, clump->atomics){
		Atomic *a = Atomic::fromClump(lnk);
		Geometry *g = a->geometry;
		Frame *f = a->getFrame();
		if(g==nil||g->numMorphTargets<1) continue;
		float mn[3],mx[3];
		MinMax(g->morphTargets[0].vertices, g->numVertices, mn, mx);
		Matrix *ltm = f->getLTM();
		float px=ltm->pos.x, py=ltm->pos.y, pz=ltm->pos.z;
		float rx=mx[0]-mn[0], ry=mx[1]-mn[1], rz=mx[2]-mn[2];
		float rad = 0.5f*(rx>ry ? (rx>rz?rx:rz) : (ry>rz?ry:rz));
		printf("%-20s (%7.4f %7.4f %7.4f)  min[%7.4f %7.4f %7.4f] max[%7.4f %7.4f %7.4f]  r=%.4f\n",
			FName(f), px,py,pz, mn[0],mn[1],mn[2], mx[0],mx[1],mx[2], rad);
		if(strcmp(FName(f),"wheel")==0){
			Sphere *bs = &g->morphTargets[0].boundingSphere;
			printf("    [stored boundingSphere center=(%.4f %.4f %.4f) r=%.4f]\n", bs->center.x, bs->center.y, bs->center.z, bs->radius);
		}
		/* model space bbox: transform local bbox corners by LTM */
		for(int cx=0;cx<2;cx++)for(int cy=0;cy<2;cy++)for(int cz=0;cz<2;cz++){
			V3d v; v.x = cx?mx[0]:mn[0]; v.y = cy?mx[1]:mn[1]; v.z = cz?mx[2]:mn[2];
			V3d w;
			w.x = ltm->right.x*v.x + ltm->up.x*v.y + ltm->at.x*v.z + ltm->pos.x;
			w.y = ltm->right.y*v.x + ltm->up.y*v.y + ltm->at.y*v.z + ltm->pos.y;
			w.z = ltm->right.z*v.x + ltm->up.z*v.y + ltm->at.z*v.z + ltm->pos.z;
			float c[3]={w.x,w.y,w.z};
			for(int k=0;k<3;k++){ if(c[k]<modelMin[k])modelMin[k]=c[k]; if(c[k]>modelMax[k])modelMax[k]=c[k]; }
			/* also without the frame position, to see the authored mesh spread */
			float o[3]={v.x,v.y,v.z};
			for(int k=0;k<3;k++){ if(o[k]<origModelMin[k])origModelMin[k]=o[k]; if(o[k]>origModelMax[k])origModelMax[k]=o[k]; }
		}
	}
	printf("\nmodel bbox (frames applied): min=(%.4f %.4f %.4f) max=(%.4f %.4f %.4f)  size=(%.4f %.4f %.4f)\n",
		modelMin[0],modelMin[1],modelMin[2], modelMax[0],modelMax[1],modelMax[2],
		modelMax[0]-modelMin[0], modelMax[1]-modelMin[1], modelMax[2]-modelMin[2]);
	printf("mesh bbox (frames ignored):  min=(%.4f %.4f %.4f) max=(%.4f %.4f %.4f)\n",
		origModelMin[0],origModelMin[1],origModelMin[2], origModelMax[0],origModelMax[1],origModelMax[2]);
	return 0;
}
