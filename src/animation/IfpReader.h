#ifndef REVC_IFP_READER_H
#define REVC_IFP_READER_H

// Bounded, allocation-free validation shared by Custom and its regression tests.
// Compact frames are engine structs (q, time, translation), NOT ANPK's
// q, translation, time. The on-disk compact quaternion is already inverted.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

namespace ifp {
inline uint32_t U32(const uint8_t *p)
{
	return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline float Float(const uint8_t *p)
{
	uint32_t bits = U32(p);
	float f;
	memcpy(&f, &bits, 4);
	return f;
}
inline bool SABone(int tag)
{
	// VC already has root, pelvis, spine, hands AND fingers (25/35).
	return tag == 6 || tag == 7 || tag == 8 || tag == 26 || tag == 36 ||
	       tag == 44 || tag == 54 || tag == 201 || tag == 301 || tag == 302;
}
struct Result {
	bool compact, mobile, sa;
	uint32_t animations;
	int names;
	Result() : compact(false), mobile(false), sa(false), animations(0), names(0) {}
};
struct Cursor {
	const uint8_t *data;
	size_t size, pos;
	Cursor(const uint8_t *d, size_t n) : data(d), size(n), pos(0) {}
	const uint8_t *Take(size_t n) {
		if(n > size - pos) return NULL;
		const uint8_t *p = data + pos;
		pos += n;
		return p;
	}
	bool Chunk(const char *id, Cursor &body) {
		const uint8_t *h = Take(8);
		if(!h || memcmp(h, id, 4)) return false;
		size_t n = U32(h + 4);
		const uint8_t *p = Take(n);
		if(!p || !Take((4 - (n & 3)) & 3)) return false;
		body = Cursor(p, n);
		return true;
	}
};
inline void Name(Result &r, const uint8_t *p, size_t n, char (*names)[24], int capacity)
{
	if(names && r.names < capacity){
		if(n > 23) n = 23;
		memcpy(names[r.names], p, n);
		names[r.names][n] = 0;
		r.names++;
	}
}
inline bool Frames(const uint8_t *p, uint32_t count, uint32_t stride, bool compact, bool packed)
{
	float previous = 0.0f;
	for(uint32_t i = 0; i < count; i++, p += stride){
		float time;
		if(packed){
			// SA uses signed 16-bit fixed point: q/4096, t/60, xyz/1024.
			time = int16_t(uint16_t(p[8]) | uint16_t(p[9]) << 8) / 60.0f;
		}else{
			for(uint32_t j = 0; j < stride; j += 4)
				if(!isfinite(Float(p + j))) return false;
			time = Float(p + (compact ? 16 : stride - 4));
		}
		if(time < previous) return false;
		previous = time;
	}
	return true;
}
inline bool Compact(const uint8_t *data, size_t size, bool mobile, Result &r, char (*names)[24], int capacity)
{
	bool anp3 = data[3] == '3';
	if(size < 36 || (mobile && !anp3)) return false;
	if(!mobile && U32(data + 4) != size - 8) return false;
	Cursor c(data, size);
	c.pos = mobile ? 4 : 8;
	const uint8_t *block = c.Take(28);
	if(!block) return false;
	uint32_t count = U32(block + 24);
	if(!count || count > 10000 || (mobile && !c.Take(4))) return false;
	r.compact = r.sa = true;
	r.mobile = mobile;
	r.animations = count;
	for(uint32_t a = 0; a < count; a++){
		const uint8_t *anim = c.Take(anp3 ? 36 : 28);
		if(!anim) return false;
		Name(r, anim, 24, names, capacity);
		uint32_t sequences = U32(anim + 24);
		if(!sequences || sequences > 4096) return false;
		for(uint32_t s = 0; s < sequences; s++){
			const uint8_t *seq = c.Take(36);
			if(!seq) return false;
			uint32_t type = U32(seq + 24), frames = U32(seq + 28);
			if(type < 1 || type > 4 || frames > 65535) return false;
			const uint32_t strides[] = {0, 20, 32, 10, 16};
			uint32_t stride = strides[type];
			const uint8_t *keys = c.Take(size_t(frames) * stride);
			if(!keys || !Frames(keys, frames, stride, true, type >= 3)) return false;
		}
	}
	return c.pos == c.size;
}
inline bool ANPK(const uint8_t *data, size_t size, Result &r, char (*names)[24], int capacity)
{
	Cursor file(data, size), root(NULL, 0), info(NULL, 0);
	if(!file.Chunk("ANPK", root) || file.pos != file.size || !root.Chunk("INFO", info) ||
	   info.size < 5 || info.size > 256 || !memchr(info.data + 4, 0, info.size - 4)) return false;
	r.animations = U32(info.data);
	if(!r.animations || r.animations > 10000) return false;
	for(uint32_t a = 0; a < r.animations; a++){
		Cursor name(NULL, 0), anim(NULL, 0);
		if(!root.Chunk("NAME", name) || !name.size || name.size > 256 ||
		   !memchr(name.data, 0, name.size) || !root.Chunk("DGAN", anim) ||
		   !anim.Chunk("INFO", info) || info.size < 4 || info.size > 256) return false;
		Name(r, name.data, name.size, names, capacity);
		uint32_t sequences = U32(info.data);
		if(!sequences || sequences > 4096) return false;
		for(uint32_t s = 0; s < sequences; s++){
			Cursor node(NULL, 0), desc(NULL, 0), keys(NULL, 0);
			if(!anim.Chunk("CPAN", node) || !node.Chunk("ANIM", desc) ||
			   (desc.size != 44 && desc.size != 40)) return false;
			uint32_t frames = U32(desc.data + 28);
			if(frames > 65535) return false;
			if(desc.size == 44 && SABone(int32_t(U32(desc.data + 40)))) r.sa = true;
			if(frames){
				if(node.size - node.pos < 8) return false;
				const char *id = reinterpret_cast<const char *>(node.data + node.pos);
				uint32_t stride = !memcmp(id, "KR00", 4) ? 20 :
				                  !memcmp(id, "KRT0", 4) ? 32 : !memcmp(id, "KRTS", 4) ? 44 : 0;
				if(!stride || !node.Chunk(id, keys) || keys.size != size_t(frames) * stride ||
				   !Frames(keys.data, frames, stride, false, false)) return false;
			}
			if(node.pos != node.size) return false;
		}
		if(anim.pos != anim.size) return false;
	}
	return root.pos == root.size;
}
inline bool Inspect(const uint8_t *data, size_t size, Result &result, char (*names)[24] = NULL, int capacity = 0)
{
	result = Result();
	if(!data || size < 8) return false;
	Result r;
	if(!memcmp(data, "ANPK", 4)){
		if(!ANPK(data, size, r, names, capacity)) return false;
	}else if(!memcmp(data, "ANP2", 4) || !memcmp(data, "ANP3", 4)){
		if(!Compact(data, size, false, r, names, capacity)){
			r = Result();
			if(!Compact(data, size, true, r, names, capacity)) return false;
		}
	}else return false;
	result = r;
	return true;
}
} // namespace ifp
#endif
