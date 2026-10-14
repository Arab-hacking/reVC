#pragma once
#include <vector>
#include <stdint.h>
#include <cstring>
using Bytes = std::vector<uint8_t>;
inline void u32(Bytes &b, uint32_t n) { for(int i=0;i<4;i++) b.push_back(n>>(8*i)); }
inline void u16(Bytes &b, uint16_t n) { b.push_back(n); b.push_back(n>>8); }
inline void f32(Bytes &b, float f) { uint32_t n; memcpy(&n,&f,4); u32(b,n); }
inline void name(Bytes &b,const char *s,size_t n=24) { size_t p=b.size(); b.resize(p+n); memcpy(b.data()+p,s,strlen(s)); }
inline void patch(Bytes &b,size_t p,uint32_t n) { for(int i=0;i<4;i++) b[p+i]=n>>(8*i); }
inline void chunk(Bytes &b,const char *id,const Bytes &body) {
	b.insert(b.end(),id,id+4); u32(b,body.size()); b.insert(b.end(),body.begin(),body.end());
	while(b.size()%4) b.push_back(0);
}
inline Bytes compact(bool anp3=true,bool mobile=false) {
	Bytes b; const char *id=anp3?"ANP3":"ANP2"; b.insert(b.end(),id,id+4);
	if(!mobile) u32(b,0);
	name(b,"ped"); u32(b,1); if(mobile) u32(b,32); // small allocation hint: old heuristic failed
	name(b,"walk_civi"); u32(b,4); if(anp3) { u32(b,156); u32(b,1); }
	for(int type=1;type<=4;type++) {
		name(b,"bone"); u32(b,type); u32(b,2); u32(b,type==1?0:type==2?1:type==3?26:44);
		for(int frame=0;frame<2;frame++) {
			if(type<3) {
				f32(b,0.5f); f32(b,0); f32(b,0); f32(b,0.5f); f32(b,frame);
				if(type==2) { f32(b,2); f32(b,3); f32(b,4); }
			} else {
				u16(b,2048); u16(b,0); u16(b,0); u16(b,2048); u16(b,frame*60);
				if(type==4) { u16(b,2048); u16(b,3072); u16(b,4096); }
			}
		}
	}
	if(!mobile) patch(b,4,b.size()-8);
	return b;
}
inline Bytes anpk(int bone=1) {
	Bytes desc; name(desc,"Pelvis",28); u32(desc,2); u32(desc,0); u32(desc,0); u32(desc,bone);
	Bytes keys;
	for(int f=0;f<2;f++) { f32(keys,0.5f); f32(keys,0); f32(keys,0); f32(keys,0.5f);
		f32(keys,2); f32(keys,3); f32(keys,4); f32(keys,f); }
	Bytes node; chunk(node,"ANIM",desc); chunk(node,"KRT0",keys);
	Bytes ai; u32(ai,1); u32(ai,0); Bytes anim; chunk(anim,"INFO",ai); chunk(anim,"CPAN",node);
	Bytes bi; u32(bi,1); name(bi,"ped"); Bytes root; chunk(root,"INFO",bi);
	Bytes n; name(n,"walk_civi",12); chunk(root,"NAME",n); chunk(root,"DGAN",anim);
	Bytes b; chunk(b,"ANPK",root); return b;
}
