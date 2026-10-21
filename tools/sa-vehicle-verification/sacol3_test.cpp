/*
 * sacol3_test.cpp - проверка парсера SA COL3 (src/collision/SACol3.h) на реальном
 * admiral.dff: чанк CollisionModel извлекается из clump extension и разбирается
 * тем же кодом, который использует reVC при загрузке SA-машины.
 *
 * build:
 *   g++ -std=c++11 -I/home/user/reVC/src sacol3_test.cpp -o sacol3_test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "collision/SACol3.h"

static int failures = 0;
#define CHECK(cond, ...) do { if(!(cond)){ failures++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while(0)

static uint32 rd32(const uint8 *p){ uint32 v; memcpy(&v,p,4); return v; }
static uint16 rd16(const uint8 *p){ uint16 v; memcpy(&v,p,2); return v; }
static float  rdf(const uint8 *p){ float v; memcpy(&v,p,4); return v; }

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/home/user/admiral_extracted/admiral.dff";
	FILE *f = fopen(path, "rb");
	if(!f){ printf("cannot open %s\n", path); return 2; }
	fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
	uint8 *file = (uint8*)malloc(len);
	if(fread(file, 1, len, f) != (size_t)len){ printf("short read\n"); return 2; }
	fclose(f);
	printf("loaded %s (%ld bytes)\n", path, len);

	/* --- найти Rockstar chunk CollisionModel (0x0253F2FA) как это делает reVC --- */
	uint32 wanted = (0x0253F2u << 8) | 0xFA;
	long found = -1;
	/* note: Rockstar chunks inside the extension are not 4-byte aligned in the file */
	for(long i = 0; i + 12 <= len - 4; i++)
		if(rd32(file+i) == wanted){ found = i; break; }
	CHECK(found > 0, "CollisionModel chunk not found in the DFF");
	if(found < 0) return 1;
	uint32 chunkSize = rd32(file+found+4);
	const uint8 *body = file + found + 12;
	printf("CollisionModel chunk at 0x%lX, body %u bytes\n", found, chunkSize);
	CHECK(memcmp(body, "COL3", 4) == 0, "chunk body does not start with COL3");
	CHECK(rd32(body+4) + 8 == chunkSize, "inner COL3 size %u + 8 != chunk size %u", rd32(body+4), chunkSize);

	/* --- разбор --- */
	SACol3 col;
	bool ok = SACol3Parse(body, chunkSize, col);
	CHECK(ok, "SACol3Parse rejected a valid COL3 block");
	if(!ok) return 1;

	printf("bounds    min (%.4f %.4f %.4f) max (%.4f %.4f %.4f)\n",
		col.boundingMin[0], col.boundingMin[1], col.boundingMin[2],
		col.boundingMax[0], col.boundingMax[1], col.boundingMax[2]);
	printf("bsphere   center (%.4f %.4f %.4f) r %.4f\n",
		col.boundingCenter[0], col.boundingCenter[1], col.boundingCenter[2], col.boundingRadius);
	printf("volumes   %u spheres, %u boxes, %u verts, %u faces\n",
		col.numSpheres, col.numBoxes, col.numVerts, col.numFaces);

	/* --- ожидаемые значения (независимый разбор тем же смещениям файла) --- */
	CHECK(col.numSpheres == 20, "numSpheres = %u, expected 20", col.numSpheres);
	CHECK(col.numBoxes == 0, "numBoxes = %u, expected 0", col.numBoxes);
	CHECK(col.numVerts == 26, "numVerts = %u, expected 26", col.numVerts);
	CHECK(col.numFaces == 22, "numFaces = %u, expected 22", col.numFaces);
	CHECK(col.boundingMin[2] == 0.0f, "model does not sit on the ground: min.z = %f", col.boundingMin[2]);
	CHECK(col.boundingMax[2] > 1.0f && col.boundingMax[2] < 2.0f, "unexpected height: max.z = %f", col.boundingMax[2]);
	CHECK(col.boundingRadius > 2.9f && col.boundingRadius < 3.1f, "bounding radius = %f", col.boundingRadius);

	/* первая сфера: центр (0.403,-2.487,0.483), r 0.372 - колесо/угол кузова */
	CHECK(rdf(col.spheres+12) > 0.36f && rdf(col.spheres+12) < 0.39f, "sphere[0] radius = %f", rdf(col.spheres+12));
	CHECK(rdf(col.spheres+0) > 0.40f && rdf(col.spheres+0) < 0.41f, "sphere[0].x = %f", rdf(col.spheres+0));

	/* сферы внутри bbox, поверхности в диапазоне VC (после маппинга) */
	int badSurf = 0, outOfBox = 0;
	for(uint32 i = 0; i < col.numSpheres; i++){
		const uint8 *p = col.spheres + i*SACOL3_SPHERE_SIZE;
		float r = rdf(p+12);
		for(int k = 0; k < 3; k++){
			float c = rdf(p+k*4);
			float lo = col.boundingMin[k] - 0.01f, hi = col.boundingMax[k] + 0.01f;
			if(c - r < lo || c + r > hi) outOfBox++;
		}
		if(p[16] > 34) badSurf++;
	}
	CHECK(outOfBox == 0, "%d sphere axes stick out of the bounding box", outOfBox);
	printf("sphere surfaces > VC range (mapped to SURFACE_CAR_PANEL): %d\n", badSurf);

	/* позиции вершин (int16/128) - все внутри bbox */
	int vOut = 0;
	for(uint32 i = 0; i < col.numVerts; i++){
		int16 *v = col.verts + i*3;
		for(int k = 0; k < 3; k++){
			float c = v[k]/128.0f;
			if(c < col.boundingMin[k]-0.01f || c > col.boundingMax[k]+0.01f) vOut++;
		}
	}
	CHECK(vOut == 0, "%d vertex components outside the bounding box", vOut);

	/* индексы граней уже проверены парсером; печатаем высоты трианглов */
	printf("first faces:");
	for(uint32 i = 0; i < col.numFaces && i < 5; i++){
		const uint8 *p = col.faces + i*SACOL3_FACE_SIZE;
		printf(" (%u,%u,%u)", rd16(p), rd16(p+2), rd16(p+4));
	}
	printf("\n");

	/* --- негативные кейсы --- */
	SACol3 tmp;
	uint8 *shortBuf = (uint8*)malloc(chunkSize);
	memcpy(shortBuf, body, chunkSize);
	CHECK(!SACol3Parse(shortBuf, 64, tmp), "short buffer was accepted");
	CHECK(!SACol3Parse(shortBuf, chunkSize-1, tmp), "truncated buffer was accepted");
	memcpy(shortBuf, "COL2", 4);
	CHECK(!SACol3Parse(shortBuf, chunkSize, tmp), "bad FourCC was accepted");
	memcpy(shortBuf, body, chunkSize);
	/* испортить индекс грани */
	uint32 offFaces = rd32(body+0x64) + 4;
	shortBuf[offFaces] = 0xFF; shortBuf[offFaces+1] = 0xFF;
	CHECK(!SACol3Parse(shortBuf, chunkSize, tmp), "out-of-range face index was accepted");
	free(shortBuf);

	printf(failures ? "\nSACOL3 TEST: %d FAILURES\n" : "\nSACOL3 TEST: all checks passed\n", failures);
	return failures ? 1 : 0;
}
