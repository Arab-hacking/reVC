#ifndef REVC_CUSTOM_DECODEDASSETCACHE_H
#define REVC_CUSTOM_DECODEDASSETCACHE_H

#include <cstddef>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>

// Cache only finished, immutable CPU data. A caller holds its own shared_ptr
// while RenderWare reads it, so LRU eviction cannot free an in-flight stream.
// No game or RenderWare dependencies: the ownership and memory cap are tested
// by the standalone custom_asset_cache_tests target.
namespace customcache {

template <typename T>
class DecodedAssetCache
{
	struct Entry {
		std::shared_ptr<const T> value;
		size_t bytes;
		std::list<std::string>::iterator lru;
		Entry(const std::shared_ptr<const T> &v, size_t n, std::list<std::string>::iterator it)
			: value(v), bytes(n), lru(it) {}
	};
	std::map<std::string, Entry> entries;
	std::list<std::string> lru; // most recently used first
	size_t limit;
	size_t used;
	size_t hits;
	size_t misses;
	size_t evictions;

	void EvictOldest(void)
	{
		const std::string &key = lru.back();
		typename std::map<std::string, Entry>::iterator it = entries.find(key);
		used -= it->second.bytes;
		entries.erase(it);
		lru.pop_back();
		evictions++;
	}

public:
	explicit DecodedAssetCache(size_t maxBytes)
		: limit(maxBytes), used(0), hits(0), misses(0), evictions(0) {}
	DecodedAssetCache(const DecodedAssetCache&) = delete;
	DecodedAssetCache& operator=(const DecodedAssetCache&) = delete;

	bool Contains(const std::string &key) const { return entries.find(key) != entries.end(); }

	std::shared_ptr<const T> Find(const std::string &key)
	{
		typename std::map<std::string, Entry>::iterator it = entries.find(key);
		if(it == entries.end()){
			misses++;
			return std::shared_ptr<const T>();
		}
		hits++;
		lru.splice(lru.begin(), lru, it->second.lru);
		return it->second.value;
	}

	bool Insert(const std::string &key, const std::shared_ptr<const T> &value, size_t bytes)
	{
		if(!value || bytes == 0 || bytes > limit)
			return false; // do not evict everything for an oversized asset
		Erase(key);
		while(used > limit - bytes)
			EvictOldest();
		lru.push_front(key);
		entries.insert(std::make_pair(key, Entry(value, bytes, lru.begin())));
		used += bytes;
		return true;
	}

	void Erase(const std::string &key)
	{
		typename std::map<std::string, Entry>::iterator it = entries.find(key);
		if(it == entries.end())
			return;
		used -= it->second.bytes;
		lru.erase(it->second.lru);
		entries.erase(it);
	}

	void Clear(void)
	{
		entries.clear();
		lru.clear();
		used = 0;
	}

	size_t Bytes(void) const { return used; }
	size_t Count(void) const { return entries.size(); }
	size_t Hits(void) const { return hits; }
	size_t Misses(void) const { return misses; }
	size_t Evictions(void) const { return evictions; }
};

} // namespace customcache

#endif
