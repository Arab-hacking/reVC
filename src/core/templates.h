#ifndef __GTA_TEMPLATES_H__
#define __GTA_TEMPLATES_H__

#include <new>
#include <climits>
#include <cstring>

template<typename T, int32 n>
class CStore
{
public:
	int32 allocPtr;
	T store[n];

	T *Alloc(void){
		if(allocPtr >= n){
			printf("Size of this thing:%d needs increasing\n", n);
			assert(0);
		}
		return &store[allocPtr++];
	}
	void Clear(void){
		allocPtr = 0;
	}
	int32 GetIndex(T *item){
		assert(item >= &store[0]);
		assert(item < &store[n]);
		return item - store;
	}
	T *GetItem(int32 index){
		assert(index >= 0);
		assert(index < n);
		return &store[index];
	}
};

#define POOLFLAG_ID     0x7f
#define POOLFLAG_ISFREE 0x80

// Fixed-size by default. Building and sector-pointer pools opt into stable,
// incremental blocks: existing CBuilding/CPtrNode addresses and sector-list
// pointers must NEVER be invalidated by a realloc during IPL loading.
template<typename T, typename U = T>
class CPool
{
    struct ExtraBlock {
        U *entries;
        uint8 *flags;
        int32 start, size;
        ExtraBlock *next;
    };

    U *m_entries;
    uint8 *m_flags;
    const char *m_name; // pool labels passed by CPools::Initialise are static
    int32 m_size, m_initialSize, m_growBy, m_allocPtr, m_used, m_storedSize;
    ExtraBlock *m_extraHead, *m_extraTail;

    uint8 &Flag(int32 index) {
        if(index >= 0 && index < m_initialSize) return m_flags[index];
        for(ExtraBlock *b = m_extraHead; b; b = b->next)
            if(index >= b->start && index - b->start < b->size)
                return b->flags[index - b->start];
        assert(0 && "pool flag index out of bounds");
        return m_flags[0];
    }
    uint8 Flag(int32 index) const {
        if(index >= 0 && index < m_initialSize) return m_flags[index];
        for(ExtraBlock *b = m_extraHead; b; b = b->next)
            if(index >= b->start && index - b->start < b->size)
                return b->flags[index - b->start];
        assert(0 && "pool flag index out of bounds");
        return 0;
    }
    U *Entry(int32 index) const {
        if(index >= 0 && index < m_initialSize) return &m_entries[index];
        for(ExtraBlock *b = m_extraHead; b; b = b->next)
            if(index >= b->start && index - b->start < b->size)
                return &b->entries[index - b->start];
        assert(0 && "pool entry index out of bounds");
        return nil;
    }
    bool Grow() {
        // Pool handles use (slot << 8) | generation. Even if a caller only
        // uses slots, expanding past the signed-handle range is not safe.
        const int32 maxSlots = INT_MAX >> 8;
        if(m_growBy <= 0 || m_size >= maxSlots) return false;
        int32 count = m_growBy < maxSlots - m_size ? m_growBy : maxSlots - m_size;
        if((size_t)count > SIZE_MAX / sizeof(U)) return false;
        ExtraBlock *b = new (std::nothrow) ExtraBlock;
        if(b == nil) return false;
        b->entries = (U*)new (std::nothrow) uint8[sizeof(U)*(size_t)count];
        b->flags = new (std::nothrow) uint8[count];
        if(b->entries == nil || b->flags == nil) {
            delete[] (uint8*)b->entries;
            delete[] b->flags;
            delete b;
            return false;
        }
        std::memset(b->flags, POOLFLAG_ISFREE, count);
        b->start = m_size;
        b->size = count;
        b->next = nil;
        if(m_extraTail) m_extraTail->next = b;
        else m_extraHead = b;
        m_extraTail = b;
        m_size += count;
        return true;
    }
    static int32 IndexIn(const U *entry, const U *base, int32 size, int32 start) {
        // Subtracting pointers into DIFFERENT allocations is undefined, even
        // for IsBuildingPointerValid, which intentionally probes other pools.
        uintptr p = (uintptr)entry, b = (uintptr)base;
        size_t bytes = sizeof(U)*(size_t)size;
        return p >= b && (size_t)(p - b) < bytes && (p - b) % sizeof(U) == 0 ?
               start + (int32)((p - b) / sizeof(U)) : -1;
    }

public:
    explicit CPool(int32 size, const char *name, int32 growBy = 0) :
        m_name(name), m_size(size), m_initialSize(size), m_growBy(growBy), m_allocPtr(-1),
        m_used(0), m_storedSize(0), m_extraHead(nil), m_extraTail(nil)
    {
        assert(size > 0);
        m_entries = (U*)new uint8[sizeof(U)*(size_t)size];
        m_flags = new uint8[size];
        std::memset(m_flags, POOLFLAG_ISFREE, size);
    }
    ~CPool() { Flush(); }
    void Flush() {
        delete[] (uint8*)m_entries;
        delete[] m_flags;
        for(ExtraBlock *b = m_extraHead; b;) {
            ExtraBlock *next = b->next;
            delete[] (uint8*)b->entries;
            delete[] b->flags;
            delete b;
            b = next;
        }
        m_entries = nil;
        m_flags = nil;
        m_extraHead = m_extraTail = nil;
        m_size = m_initialSize = m_growBy = m_used = m_storedSize = 0;
        m_allocPtr = -1;
    }

    int GetId(int i) const { return i < 0 || i >= m_size ? 0 : Flag(i) & POOLFLAG_ID; }
    bool GetIsFree(int i) const { return i < 0 || i >= m_size || !!(Flag(i) & POOLFLAG_ISFREE); }
    void SetId(int i, int id) {
        Flag(i) = (Flag(i) & POOLFLAG_ISFREE) | (id & POOLFLAG_ID);
    }
    void SetIsFree(int i, bool isFree) {
        uint8 &flag = Flag(i);
        if(!!(flag & POOLFLAG_ISFREE) != isFree)
            m_used += isFree ? -1 : 1;
        if(isFree) flag |= POOLFLAG_ISFREE;
        else flag &= ~POOLFLAG_ISFREE;
    }
    int32 GetSize(void) const { return m_size; }
    int32 GetNoOfUsedSpaces(void) const { return m_used; }
    int32 GetNoOfFreeSpaces(void) const { return m_size - m_used; }
    bool EnsureFree(int32 count) {
        if(count < 0) return false;
        while(GetNoOfFreeSpaces() < count)
            if(!Grow()) return false;
        return true;
    }
    T *New(void) {
        if(m_used == m_size) {
            int32 oldSize = m_size;
            if(!Grow()) return nil;
            m_allocPtr = oldSize - 1;
        }
        for(int32 examined = 0; examined < m_size; examined++) {
            if(++m_allocPtr >= m_size) m_allocPtr = 0;
            if(!GetIsFree(m_allocPtr)) continue;
            SetIsFree(m_allocPtr, false);
            SetId(m_allocPtr, GetId(m_allocPtr)+1);
            return (T*)Entry(m_allocPtr);
        }
        assert(0 && "pool free-space count is inconsistent");
        return nil;
    }
    T *New(int32 handle) {
        int32 index = handle >> 8;
        if(index < 0 || index >= m_size) return nil;
        SetNotFreeAt(handle);
        return (T*)Entry(index);
    }
    void SetNotFreeAt(int32 handle) {
        int32 index = handle >> 8;
        if(index < 0 || index >= m_size) { assert(0); return; }
        SetIsFree(index, false);
        SetId(index, handle & POOLFLAG_ID);
        for(m_allocPtr = 0; m_allocPtr < m_size; m_allocPtr++)
            if(GetIsFree(m_allocPtr)) return;
    }
    void Delete(T *entry) {
        int32 i = GetJustIndex(entry);
        if(i < 0) return; // a bad pointer must not free an unrelated slot in Release
        SetIsFree(i, true);
        if(i < m_allocPtr) m_allocPtr = i;
    }
    T *GetSlot(int i) {
        return i < 0 || i >= m_size || GetIsFree(i) ? nil : (T*)Entry(i);
    }
    T *GetAt(int handle) {
        int32 index = handle >> 8;
        return index < 0 || index >= m_size || Flag(index) != (handle & 0xFF) ?
               nil : (T*)Entry(index);
    }
    int32 GetIndex(T *entry) {
        int32 i = GetJustIndex(entry);
        return i < 0 ? -1 : Flag(i) + (i << 8);
    }
    // Safe for stale/foreign pointers: no dereference of the entry and no
    // pointer subtraction between allocations. Used by ped validity checks
    // BEFORE reading fields or requesting a pool handle.
    bool IsAllocated(T *entry) {
        int32 i = GetJustIndex_NoFreeAssert(entry);
        return i >= 0 && i < m_size && (U*)entry == Entry(i) && !GetIsFree(i);
    }
    int32 GetJustIndex(T *entry) {
        int32 i = GetJustIndex_NoFreeAssert(entry);
        if(i < 0 || i >= m_size || (U*)entry != Entry(i) || GetIsFree(i)) {
            debug("CPool %s: invalid pointer %p, slot=%d, size=%d, free=%d, used=%d\n",
                m_name, (void*)entry, i, m_size,
                i >= 0 && i < m_size ? (int)GetIsFree(i) : -1, m_used);
            assert(0 && "pool pointer is not an allocated slot");
            return -1;
        }
        return i;
    }
    int32 GetJustIndex_NoFreeAssert(T *entry) {
        int32 i = IndexIn((U*)entry, m_entries, m_initialSize, 0);
        if(i >= 0) return i;
        for(ExtraBlock *b = m_extraHead; b; b = b->next) {
            i = IndexIn((U*)entry, b->entries, b->size, b->start);
            if(i >= 0) return i;
        }
        return -1;
    }
    void ClearStorage(uint8 *&flags, U *&entries) {
        delete[] flags;
        delete[] (uint8*)entries;
        flags = nil;
        entries = nil;
    }
    uint32 GetMaxEntrySize() const { return sizeof(U); }
    void Store(uint8 *&flags, U *&entries) {
        flags = new uint8[m_size];
        entries = (U*)new uint8[sizeof(U)*(size_t)m_size];
        std::memcpy(flags, m_flags, m_initialSize);
        std::memcpy(entries, m_entries, sizeof(U)*(size_t)m_initialSize);
        for(ExtraBlock *b = m_extraHead; b; b = b->next) {
            std::memcpy(flags+b->start, b->flags, b->size);
            std::memcpy(entries+b->start, b->entries, sizeof(U)*(size_t)b->size);
        }
        m_storedSize = m_size;
        debug("Stored:%d (/%d)\n", GetNoOfUsedSpaces(), m_size);
    }
    void CopyBack(uint8 *&flags, U *&entries) {
        int32 saved = m_storedSize;
        assert(flags && entries && saved >= m_initialSize && saved <= m_size);
        std::memcpy(m_flags, flags, m_initialSize);
        std::memcpy(m_entries, entries, sizeof(U)*(size_t)m_initialSize);
        for(ExtraBlock *b = m_extraHead; b; b = b->next) {
            int32 n = saved > b->start ?
                      (saved - b->start < b->size ? saved - b->start : b->size) : 0;
            if(n) {
                std::memcpy(b->flags, flags+b->start, n);
                std::memcpy(b->entries, entries+b->start, sizeof(U)*(size_t)n);
            }
            // If the pool grew DURING replay, no saved sector list can point
            // into those new blocks. Keep them but mark the slots free.
            std::memset(b->flags+n, POOLFLAG_ISFREE, b->size-n);
        }
        m_used = 0;
        for(int32 i = 0; i < m_size; i++)
            if(!GetIsFree(i)) m_used++;
        m_allocPtr = 0;
        m_storedSize = 0;
        ClearStorage(flags, entries);
        debug("CopyBack:%d (/%d)\n", GetNoOfUsedSpaces(), m_size);
    }
};

template<typename T>
class CLink
{
public:
	T item;
	CLink<T> *prev;
	CLink<T> *next;

	void Insert(CLink<T> *link){
		link->next = this->next;
		this->next->prev = link;
		link->prev = this;
		this->next = link;
	}
	void Remove(void){
		this->prev->next = this->next;
		this->next->prev = this->prev;
	}
};

template<typename T>
class CLinkList
{
public:
	CLink<T> head, tail;
	CLink<T> freeHead, freeTail;
	CLink<T> *links;

	void Init(int n){
		links = new CLink<T>[n];
		head.next = &tail;
		tail.prev = &head;
		freeHead.next = &freeTail;
		freeTail.prev = &freeHead;
		while(n--)
			freeHead.Insert(&links[n]);
	}
	void Shutdown(void){
		delete[] links;
		links = nil;
	}
	void Clear(void){
		while(head.next != &tail)
			Remove(head.next);
	}
	CLink<T> *Insert(T const &item){
		CLink<T> *node = freeHead.next;
		if(node == &freeTail)
			return nil;
		node->item = item;
		node->Remove();		// remove from free list
		head.Insert(node);
		return node;
	}
	CLink<T> *InsertSorted(T const &item){
		CLink<T> *sort;
		for(sort = head.next; sort != &tail; sort = sort->next)
			if(sort->item.sort >= item.sort)
				break;
		CLink<T> *node = freeHead.next;
		if(node == &freeTail)
			return nil;
		node->item = item;
		node->Remove();		// remove from free list
		sort->prev->Insert(node);
		return node;
	}
	void Remove(CLink<T> *link){
		link->Remove();		// remove from list
		freeHead.Insert(link);	// insert into free list
	}
	int32 Count(void){
		int n = 0;
		CLink<T> *lnk;
		for(lnk = head.next; lnk != &tail; lnk = lnk->next)
			n++;
		return n;
	}
};

#endif // __GTA_TEMPLATES_H__
