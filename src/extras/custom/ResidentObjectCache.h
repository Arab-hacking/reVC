#ifndef REVC_CUSTOM_RESIDENTOBJECTCACHE_H
#define REVC_CUSTOM_RESIDENTOBJECTCACHE_H

#include <cstddef>
#include <list>
#include <map>
#include <string>
#include <utility>

// Owns detached, fully constructed objects (TXD dictionaries in the game).
// Store transfers ownership ONLY on success; Take transfers it back to the
// caller. The deleter is called for LRU eviction and Clear, never for a Take.
namespace customcache {

template <typename T>
class ResidentObjectCache
{
	struct Entry {
		std::string name;
		T *object;
		size_t bytes;
		std::list<int>::iterator lru;
		Entry(const std::string &n, T *p, size_t b, std::list<int>::iterator i)
			: name(n), object(p), bytes(b), lru(i) {}
	};
	std::map<int, Entry> entries;
	std::list<int> lru;
	void (*destroy)(T*);
	size_t limit;
	size_t used;
	size_t hits;
	size_t misses;
	size_t evictions;

	void EvictOldest(void)
	{
		int slot = lru.back();
		Entry &e = entries.find(slot)->second;
		used -= e.bytes;
		destroy(e.object);
		entries.erase(slot);
		lru.pop_back();
		evictions++;
	}

public:
	ResidentObjectCache(size_t maxBytes, void (*deleter)(T*))
		: destroy(deleter), limit(maxBytes), used(0), hits(0), misses(0), evictions(0) {}
	ResidentObjectCache(const ResidentObjectCache&) = delete;
	ResidentObjectCache& operator=(const ResidentObjectCache&) = delete;
	~ResidentObjectCache(void) { Clear(); }

	bool Store(int slot, const std::string &name, T *object, size_t bytes)
	{
		if(!object || bytes == 0 || bytes > limit)
			return false;
		// Drop a previous entry for this slot before taking new ownership.
		Erase(slot);
		while(used > limit - bytes)
			EvictOldest();
		lru.push_front(slot);
		entries.insert(std::make_pair(slot, Entry(name, object, bytes, lru.begin())));
		used += bytes;
		return true;
	}

	T *Take(int slot, const std::string &name)
	{
		typename std::map<int, Entry>::iterator it = entries.find(slot);
		if(it == entries.end()){
			misses++;
			return nullptr;
		}
		if(it->second.name != name){
			Erase(slot); // slot reused under another name; never return stale pixels
			misses++;
			return nullptr;
		}
		T *object = it->second.object;
		used -= it->second.bytes;
		lru.erase(it->second.lru);
		entries.erase(it);
		hits++;
		return object;
	}

	void Erase(int slot)
	{
		typename std::map<int, Entry>::iterator it = entries.find(slot);
		if(it == entries.end())
			return;
		used -= it->second.bytes;
		destroy(it->second.object);
		lru.erase(it->second.lru);
		entries.erase(it);
	}

	void Clear(void)
	{
		while(!lru.empty())
			EvictOldest();
	}

	size_t Bytes(void) const { return used; }
	size_t Count(void) const { return entries.size(); }
	size_t Hits(void) const { return hits; }
	size_t Misses(void) const { return misses; }
	size_t Evictions(void) const { return evictions; }
};

} // namespace customcache

#endif
