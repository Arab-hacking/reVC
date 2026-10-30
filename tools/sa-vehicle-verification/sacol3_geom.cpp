// sacol3_geom.cpp - prints the collision geometry stored inside admiral.dff and
// compares it with the wheel geometry of the same model, to find out whether the
// car's collision touches the road before its tyres do.
//
// build: g++ -std=c++11 -I/home/user/reVC/src sacol3_geom.cpp -o sacol3_geom
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "collision/SACol3.h"

static uint32 rd32(const uint8 *p){ uint32 v; memcpy(&v,p,4); return v; }
static uint16 rd16(const uint8 *p){ uint16 v; memcpy(&v,p,2); return v; }
static float  rdf(const uint8 *p){ float v; memcpy(&v,p,4); return v; }

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/home/user/admiral_extracted/admiral.dff";
	// wheel data measured with geodump (out_geodump.txt)
	const float dummyZ = 0.1770f, wheelRadius = 0.3901f, bodyLowest = -0.2132f;

	FILE *f = fopen(path, "rb");
	if(!f){ printf("cannot open %s\n", path); return 2; }
	fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
	uint8 *file = (uint8*)malloc(len);
	if(fread(file, 1, len, f) != (size_t)len){ printf("short read\n"); return 2; }
	fclose(f);

	uint32 wanted = (0x0253F2u << 8) | 0xFA;
	long found = -1;
	for(long i = 0; i + 12 <= len - 4; i++)
		if(rd32(file+i) == wanted){ found = i; break; }
	if(found < 0){ printf("no CollisionModel chunk\n"); return 1; }
	uint32 chunkSize = rd32(file+found+4);
	const uint8 *body = file + found + 12;

	SACol3 col;
	if(!SACol3Parse(body, chunkSize, col)){ printf("parse failed\n"); return 1; }

	printf("admiral.dff collision block: %u spheres, %u boxes, %u verts, %u faces\n",
		col.numSpheres, col.numBoxes, col.numVerts, col.numFaces);
	printf("header bounds  min (%.4f %.4f %.4f)  max (%.4f %.4f %.4f)\n\n",
		col.boundingMin[0], col.boundingMin[1], col.boundingMin[2],
		col.boundingMax[0], col.boundingMax[1], col.boundingMax[2]);

	float lowest = 1.0e9f, lowestSphere = 1.0e9f, lowestVert = 1.0e9f;
	int i;

	printf("spheres (model space, z is up):\n");
	for(i = 0; i < (int)col.numSpheres; i++){
		const uint8 *s = col.spheres + i*SACOL3_SPHERE_SIZE;
		float x = rdf(s), y = rdf(s+4), z = rdf(s+8), r = rdf(s+12);
		float bottom = z - r;
		printf("  [%2d] center (%+7.3f %+7.3f %+7.3f)  r %.3f  bottom %+7.3f%s\n",
			i, x, y, z, r, bottom, bottom < bodyLowest ? "   <-- reaches below the tyres!" : "");
		if(bottom < lowestSphere) lowestSphere = bottom;
		if(bottom < lowest) lowest = bottom;
	}

	if(col.numBoxes){
		printf("boxes:\n");
		for(i = 0; i < (int)col.numBoxes; i++){
			const uint8 *b = col.boxes + i*SACOL3_BOX_SIZE;
			float zmin = rdf(b+8), zmax = rdf(b+20);
			printf("  [%2d] z %+7.3f .. %+7.3f%s\n", i, zmin, zmax,
				zmin < bodyLowest ? "   <-- reaches below the tyres!" : "");
			if(zmin < lowest) lowest = zmin;
		}
	}

	printf("vertices used by the face list: ");
	for(i = 0; i < (int)col.numVerts; i++){
		int16 *v = col.verts + i*3;
		float z = v[2]/128.0f;
		if(z < lowestVert) lowestVert = z;
		if(z < lowest) lowest = z;
	}
	printf("lowest z %+7.3f%s\n\n", lowestVert,
		lowestVert < bodyLowest ? "   <-- reaches below the tyres!" : "");

	printf("collision geometry lowest point : %+7.3f\n", lowest);
	printf("tyre bottom of the model        : %+7.3f  (= dummy z %.3f - radius %.3f)\n",
		bodyLowest, dummyZ, wheelRadius);
	printf("delta (collision - tyres)       : %+7.3f  %s\n", lowest - bodyLowest,
		lowest < bodyLowest - 0.005f ?
			"<-- the body would rest on this, the wheels stay in the air" :
			"(the tyres are the lowest part of the car, as in SA)");

	return 0;
}
