#include "../../src/animation/IfpReader.h"
#include "fixtures.h"
#include <cassert>
#include <cstdio>
int main() {
	ifp::Result r; char names[2][24];
	for(auto b : {compact(false),compact(),compact(true,true),anpk(),anpk(26)}) {
		assert(ifp::Inspect(b.data(),b.size(),r,names,2));
		assert(r.animations==1 && r.names==1 && !strcmp(names[0],"walk_civi"));
		for(size_t n=0;n<b.size();n++) assert(!ifp::Inspect(b.data(),n,r));
		b.push_back(0); assert(!ifp::Inspect(b.data(),b.size(),r));
	}
	auto b=compact(true,true); assert(ifp::Inspect(b.data(),b.size(),r) && r.mobile);
	memmove(b.data()+8,b.data()+4,28); patch(b,4,b.size()-8);
	assert(ifp::Inspect(b.data(),b.size(),r) && !r.mobile);
	b=anpk(); assert(ifp::Inspect(b.data(),b.size(),r) && !r.sa);
	b=anpk(26); assert(ifp::Inspect(b.data(),b.size(),r) && r.sa);
	b=compact(); patch(b,72+28,0xffffffff); assert(!ifp::Inspect(b.data(),b.size(),r));
	b=compact(); patch(b,72+24,5); assert(!ifp::Inspect(b.data(),b.size(),r));
	b=compact(); patch(b,72+36,0x7fc00000); assert(!ifp::Inspect(b.data(),b.size(),r));
	b=compact(); patch(b,72+36+20+16,0xbf800000); assert(!ifp::Inspect(b.data(),b.size(),r));
	assert(!ifp::SABone(25) && !ifp::SABone(35) && ifp::SABone(26));
	// Deterministic mutation smoke test under ASan/UBSan.
	uint32_t seed=123;
	for(int i=0;i<10000;i++) {
		b=(i&1)?compact():anpk(); seed=seed*1664525+1013904223;
		b[seed%b.size()]^=uint8_t(seed>>24); ifp::Inspect(b.data(),b.size(),r,names,2);
	}
	puts("IFP validation: PASS (ANP2/ANP3/mobile/ANPK, truncations, corrupt fields, mutations)");
}
