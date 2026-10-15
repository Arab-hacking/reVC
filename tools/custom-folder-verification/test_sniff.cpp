// Test for the ANP2/ANP3 animation package sniffer (br::SniffAnimNames):
// runs it against a real .ifp/.ani file - the Black Russia common.zip
// carries "anim/ped.ani", the size-less SA mobile layout - and prints what
// the game would decide about it.
//
//     ./test_sniff <file.ifp or file.ani>
#include "common.h"

#include <string.h>
#include <stdlib.h>

#include "brformats.h"

int
main(int argc, char *argv[])
{
	if(argc < 2){
		printf("usage: %s <file.ifp|.ani>\n", argv[0]);
		return 2;
	}
	FILE *f = fopen(argv[1], "rb");
	if(f == nil){
		printf("cannot open %s\n", argv[1]);
		return 2;
	}
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	std::vector<uint8> d(n);
	if(fread(d.data(), 1, n, f) != (size_t)n){
		printf("short read\n");
		fclose(f);
		return 2;
	}
	fclose(f);

	static char names[8192][24];
	int nn = 0;
	bool clean = br::SniffAnimNames(d.data(), d.size(), names, 8192, nn);
	printf("%s: %ld bytes, walk %s, %d animation(s)\n", argv[1], n, clean ? "clean" : "FAILED", nn);
	if(!clean)
		return 1;
	for(int i = 0; i < nn && i < 8; i++)
		printf("  %s\n", names[i]);
	if(nn > 8)
		printf("  ... and %d more\n", nn - 8);

	// what the game does with a ped package: this game needs its own names
	bool walk = false, idle = false;
	for(int i = 0; i < nn; i++){
		if(strcmp(names[i], "walk_civi") == 0) walk = true;
		if(strcmp(names[i], "idle_stance") == 0) idle = true;
	}
	printf("walk_civi: %s, idle_stance: %s -> %s\n",
		walk ? "present" : "ABSENT", idle ? "present" : "ABSENT",
		(walk && idle) ? "compatible with this game"
		               : "NOT compatible - the game's own PED.IFP stays (logged in debug.log)");
	return 0;
}
