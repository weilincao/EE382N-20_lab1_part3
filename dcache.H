/*
 * Copyright 2002-2020 Intel Corporation.
 * 
 * This software is provided to you as Sample Source Code as defined in the accompanying
 * End User License Agreement for the Intel(R) Software Development Products ("Agreement")
 * section 1.L.
 * 
 * This software and the related documents are provided as is, with no express or implied
 * warranties, other than those that are expressly stated in the License.
 */

/*! @file
 *  This file contains a configurable cache class
 */

#ifndef PIN_CACHE_H
#define PIN_CACHE_H


#define KILO 1024
#define MEGA (KILO*KILO)
#define GIGA (KILO*MEGA)

typedef UINT64 CACHE_STATS; // type of cache hit/miss counters

#include <sstream>
using std::string;
using std::ostringstream;
/*! RMR (rodric@gmail.com) 
 *   - temporary work around because decstr()
 *     casts 64 bit ints to 32 bit ones
 */
static string mydecstr(UINT64 v, UINT32 w)
{
    ostringstream o;
    o.width(w);
    o << v;
    string str(o.str());
    return str;
}

/*!
 *  @brief Checks if n is a power of 2.
 *  @returns true if n is power of 2
 */
static inline bool IsPower2(UINT32 n)
{
    return ((n & (n - 1)) == 0);
}

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline INT32 FloorLog2(UINT32 n)
{
    INT32 p = 0;

    if (n == 0) return -1;

    if (n & 0xffff0000) { p += 16; n >>= 16; }
    if (n & 0x0000ff00)	{ p +=  8; n >>=  8; }
    if (n & 0x000000f0) { p +=  4; n >>=  4; }
    if (n & 0x0000000c) { p +=  2; n >>=  2; }
    if (n & 0x00000002) { p +=  1; }

    return p;
}

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline INT32 CeilLog2(UINT32 n)
{
    return FloorLog2(n - 1) + 1;
}

/*!
 *  @brief Cache tag - self clearing on creation
 */
class CACHE_TAG
{
  private:
    ADDRINT _tag;
  public:
    bool dirty;
    int LRU;
    CACHE_TAG(ADDRINT tag = 0) { _tag = tag; dirty = false; LRU = 0; }
    bool operator==(const CACHE_TAG &right) const { return _tag == right._tag; }
    operator ADDRINT() const { return _tag; }
};


/*!
 * Everything related to cache sets
 */
namespace CACHE_SET
{

/*!
 *  @brief Cache set direct mapped
 */
class DIRECT_MAPPED
{
  private:
    CACHE_TAG _tag;

  public:
    DIRECT_MAPPED(UINT32 associativity = 1) { ASSERTX(associativity == 1); }

    VOID SetAssociativity(UINT32 associativity) { ASSERTX(associativity == 1); }
    UINT32 GetAssociativity(UINT32 associativity) { return 1; }

    UINT32 Find(CACHE_TAG tag) { return(_tag == tag); }
    VOID Replace(CACHE_TAG tag) { _tag = tag; }
};

/*!
 *  @brief Cache set with round robin replacement
 */
template <UINT32 MAX_ASSOCIATIVITY = 4>
class LRU
{
  private:
    CACHE_TAG _tags[MAX_ASSOCIATIVITY];
    UINT32 _tagsLastIndex;

  public:
    LRU(UINT32 associativity = MAX_ASSOCIATIVITY)
      : _tagsLastIndex(associativity - 1)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            _tags[index] = CACHE_TAG(0);
        }
    }

    VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
    }
    UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }
    
    UINT32 Find(CACHE_TAG tag)
    {
        bool result = false;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            if(_tags[index] == tag) { 
               result = true;
               _tags[index].LRU = 0;
            } else {
               _tags[index].LRU++;
            }
        }
        return result;
    }

    VOID Replace(CACHE_TAG tag)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
        UINT32 lru_index = _tagsLastIndex;
        int lru_val = 0; 
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            if(_tags[index].LRU > lru_val) {
               lru_index = index;
               lru_val = _tags[index].LRU;
            }
        }

        _tags[lru_index] = tag;
        _tags[lru_index].LRU = 0;
        // condition typically faster than modulo
    }
};

} // namespace CACHE_SET

namespace CACHE_ALLOC
{
    typedef enum 
    {
        STORE_ALLOCATE,
        STORE_NO_ALLOCATE
    } STORE_ALLOCATION;
}

/*!
 *  @brief Generic cache base class; no allocate specialization, no cache set specialization
 */
class CACHE_BASE
{
  public:
    // types, constants
    typedef enum 
    {
        ACCESS_TYPE_LOAD,
        ACCESS_TYPE_STORE,
        ACCESS_TYPE_NUM
    } ACCESS_TYPE;

    typedef enum
    {
        CACHE_TYPE_ICACHE,
        CACHE_TYPE_DCACHE,
        CACHE_TYPE_NUM
    } CACHE_TYPE;

  protected:
    static const UINT32 HIT_MISS_NUM = 2;
    CACHE_STATS _access[ACCESS_TYPE_NUM][HIT_MISS_NUM];
    //added for L2 cache
    CACHE_STATS _l2_access[ACCESS_TYPE_NUM][HIT_MISS_NUM];

  private:    // input params
    const std::string _name;
    const UINT32 _cacheSize;
    const UINT32 _lineSize;
    const UINT32 _associativity;
    //added
    const UINT32 _l2_cacheSize;
    const UINT32 _l2_lineSize;
    const UINT32 _l2_associativity;

    // computed params
    const UINT32 _lineShift;
    const UINT32 _setIndexMask;
    //added
    const UINT32 _l2_lineShift;
    const UINT32 _l2_setIndexMask;

    CACHE_STATS SumAccess(bool hit, int cache_level) const
    {
        CACHE_STATS sum = 0;
        /*
        if(cache_level == 1) {
          for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
          {
            sum += _access[accessType][hit];
          }
        }
        else if(cache_level == 2) {
          for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
          {
            sum += _l2_access[accessType][hit];
          }
        }
       
        */
        return sum;
    }

  protected:
    UINT32 NumSets() const { return _setIndexMask + 1; }
    UINT32 l2_NumSets() const { return _l2_setIndexMask + 1; }

  public:
    // constructors/destructors added
    CACHE_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT32 l2_cacheSize, UINT32 l2_lineSize, UINT32 l2_associativity);

    // accessors
    UINT32 CacheSize() const { return _cacheSize; }
    UINT32 LineSize() const { return _lineSize; }
    UINT32 Associativity() const { return _associativity; }
    //
    CACHE_STATS Hits(ACCESS_TYPE accessType) const { return _access[accessType][true];}
    CACHE_STATS Misses(ACCESS_TYPE accessType) const { return _access[accessType][false];}
    CACHE_STATS Accesses(ACCESS_TYPE accessType) const { return Hits(accessType) + Misses(accessType);}
    CACHE_STATS Hits() const { return SumAccess(true,1);}
    CACHE_STATS Misses() const { return SumAccess(false,1);}
    CACHE_STATS Accesses() const { return Hits() + Misses();}
    //added
    CACHE_STATS l2_Hits(ACCESS_TYPE accessType) const { return _l2_access[accessType][true];}
    CACHE_STATS l2_Misses(ACCESS_TYPE accessType) const { return _l2_access[accessType][false];}
    CACHE_STATS l2_Accesses(ACCESS_TYPE accessType) const { return l2_Hits(accessType) + l2_Misses(accessType);}
    CACHE_STATS l2_Hits() const { return SumAccess(true,2);}
    CACHE_STATS l2_Misses() const { return SumAccess(false,2);}
    CACHE_STATS l2_Accesses() const { return l2_Hits() + l2_Misses();}

    VOID SplitAddress(const ADDRINT addr, CACHE_TAG & tag, UINT32 & setIndex, UINT32 level) const
    {
        
        tag = addr >> ( (level==1)? _lineShift : _l2_lineShift);
        setIndex = tag & ( (level==1)? _setIndexMask : _l2_setIndexMask);
    }

    VOID SplitAddress(const ADDRINT addr, CACHE_TAG & tag, UINT32 & setIndex, UINT32 & lineIndex, UINT32 level) const
    {
        if (level==1 || level ==0)
        {
          const UINT32 lineMask = _lineSize - 1;
          lineIndex = addr & lineMask;
          SplitAddress(addr, tag, setIndex,1);
        }
        else if (level==2)
        {
          const UINT32 lineMask = _l2_lineSize - 1;
          lineIndex = addr & lineMask;
          SplitAddress(addr, tag, setIndex,2);
        }  

    }


    string StatsLong(string prefix = "", CACHE_TYPE = CACHE_TYPE_DCACHE) const;
};

CACHE_BASE::CACHE_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT32 l2_cacheSize, UINT32 l2_lineSize, UINT32 l2_associativity)
  : _name(name),
    _cacheSize(cacheSize),
    _lineSize(lineSize),
    _associativity(associativity),    
    _l2_cacheSize(l2_cacheSize),
    _l2_lineSize(l2_lineSize),
    _l2_associativity(l2_associativity),
    _lineShift(FloorLog2(lineSize)),
    _setIndexMask((cacheSize / (associativity * lineSize)) - 1),

    _l2_lineShift(FloorLog2(l2_lineSize)),
    _l2_setIndexMask((l2_cacheSize / (l2_associativity * l2_lineSize)) - 1)
{

    ASSERTX(IsPower2(_lineSize));
    ASSERTX(IsPower2(_setIndexMask + 1));

    for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
    {
        _access[accessType][false] = 0;
        _access[accessType][true] = 0;
        //added
        _l2_access[accessType][false] = 0;
        _l2_access[accessType][true] = 0;

    }
}

/*!
 *  @brief Stats output method
 */

string CACHE_BASE::StatsLong(string prefix, CACHE_TYPE cache_type) const
{
    const UINT32 headerWidth = 19;
    const UINT32 numberWidth = 12;

    string out;
    
    out += prefix + _name + ":" + "\n";

    if (cache_type != CACHE_TYPE_ICACHE) {
       for (UINT32 i = 0; i < ACCESS_TYPE_NUM; i++)
       {
           const ACCESS_TYPE accessType = ACCESS_TYPE(i);

           std::string type(accessType == ACCESS_TYPE_LOAD ? "Load" : "Store");

           out += prefix + ljstr(type + "-Hits:      ", headerWidth)
                  + mydecstr(l2_Hits(accessType), numberWidth)  +
                  "  " +fltstr(100.0 * l2_Hits(accessType) / l2_Accesses(accessType), 2, 6) + "%\n";

           out += prefix + ljstr(type + "-Misses:    ", headerWidth)
                  + mydecstr(l2_Misses(accessType), numberWidth) +
                  "  " +fltstr(100.0 * l2_Misses(accessType) / l2_Accesses(accessType), 2, 6) + "%\n";
        
           out += prefix + ljstr(type + "-Accesses:  ", headerWidth)
                  + mydecstr(l2_Accesses(accessType), numberWidth) +
                  "  " +fltstr(100.0 * l2_Accesses(accessType) / l2_Accesses(accessType), 2, 6) + "%\n";
        
           out += prefix + "\n";
       }
    }

    //added for L2 cache
    /*
    out += prefix + "L2 Data Cache:" + "\n";
    if (cache_type != CACHE_TYPE_ICACHE) {
       for (UINT32 i = 0; i < ACCESS_TYPE_NUM; i++)
       {
           const ACCESS_TYPE accessType = ACCESS_TYPE(i);

           std::string type(accessType == ACCESS_TYPE_LOAD ? "Load" : "Store");

           out += prefix + ljstr(type + "-Hits:      ", headerWidth)
                  + mydecstr(Hits(accessType), numberWidth)  +
                  "  " +fltstr(100.0 * Hits(accessType) / Accesses(accessType), 2, 6) + "%\n";

           out += prefix + ljstr(type + "-Misses:    ", headerWidth)
                  + mydecstr(Misses(accessType), numberWidth) +
                  "  " +fltstr(100.0 * Misses(accessType) / Accesses(accessType), 2, 6) + "%\n";
        
           out += prefix + ljstr(type + "-Accesses:  ", headerWidth)
                  + mydecstr(Accesses(accessType), numberWidth) +
                  "  " +fltstr(100.0 * Accesses(accessType) / Accesses(accessType), 2, 6) + "%\n";
        
           out += prefix + "\n";
       }
    }
    */
    out += prefix + ljstr("Total-Hits:      ", headerWidth)
           + mydecstr(Hits(), numberWidth) +
           "  " +fltstr(100.0 * Hits() / Accesses(), 2, 6) + "%\n";

    out += prefix + ljstr("Total-Misses:    ", headerWidth)
           + mydecstr(Misses(), numberWidth) +
           "  " +fltstr(100.0 * Misses() / Accesses(), 2, 6) + "%\n";

    out += prefix + ljstr("Total-Accesses:  ", headerWidth)
           + mydecstr(Accesses()+l2_Accesses(), numberWidth) +
           "  " +fltstr(100.0 * Accesses() / Accesses(), 2, 6) + "%\n";
    out += "\n";

    return out;
}


/*!
 *  @brief Templated cache class with specific cache set allocation policies
 *
 *  All that remains to be done here is allocate and deallocate the right
 *  type of cache sets.
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
class CACHE : public CACHE_BASE
{
  private:
    SET _sets[MAX_SETS];
    SET _l2_sets[MAX_SETS];

  public:
    // constructors/destructors
    CACHE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity,UINT32 l2_cacheSize, UINT32 l2_lineSize, UINT32 l2_associativity)
      : CACHE_BASE(name, cacheSize, lineSize, associativity,l2_cacheSize, l2_lineSize, l2_associativity)
    {
        ASSERTX(NumSets() <= MAX_SETS);

        for (UINT32 i = 0; i < NumSets(); i++)
        {
            _sets[i].SetAssociativity(associativity);
        }
        //added
        for (UINT32 i = 0; i < l2_NumSets(); i++)
        {
            _l2_sets[i].SetAssociativity(l2_associativity);
        }

    }

    // modifiers
    /// Cache access from addr to addr+size-1
    bool Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);
    /// Cache access at addr that does not span cache lines
    bool AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType);
};

/*!
 *  @return true if all accessed cache lines hit
 */

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
bool CACHE<SET,MAX_SETS,STORE_ALLOCATION>::Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    const ADDRINT highAddr = addr + size;
    bool allHit = true;

    const ADDRINT lineSize = LineSize();
    const ADDRINT notLineMask = ~(lineSize - 1);
    do
    {
        //bool localHit = AccessSingleLine(addr,accessType); //hit&miss are now counted inside AccessSingleLine function individually
        bool localHit = true;
        allHit &= localHit;
        addr = (addr & notLineMask) + lineSize; // start of next cache line
    }
    while (addr < highAddr);

    //_access[accessType][allHit]++;

    return allHit;
}

/*!
 *  @return true if accessed cache line hits
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
bool CACHE<SET,MAX_SETS,STORE_ALLOCATION>::AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType)
{
    CACHE_TAG tag;
    UINT32 setIndex;

    SplitAddress(addr, tag, setIndex,1);

    SET & set = _sets[setIndex];

    bool hit = set.Find(tag);

    // on miss, loads always allocate, stores optionally
    if ( (! hit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    {
        set.Replace(tag);
    }

    _access[accessType][hit]++;

    return hit;
    /*
    CACHE_TAG tag;
    UINT32 setIndex;
    
    CACHE_TAG l2_tag;
    
    SplitAddress(addr, tag, setIndex,1);

    SET & set = _sets[setIndex];

    bool hit = set.Find(tag);

    // on miss, loads always allocate, stores optionally
    if ( (! hit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    {
        
        set.Replace(tag);
        //bring L1 stuff into L2
        UINT32 l2_setIndex;
        SplitAddress(addr, l2_tag, l2_setIndex,2);
        SET & l2_set = _l2_sets[l2_setIndex];
        
        bool l2_hit = l2_set.Find(l2_tag);
        if(l2_hit==false)
          l2_set.Replace(l2_tag);
    }

    _access[accessType][hit]++;

    //added
    if (hit == false) {
      UINT32 l2_setIndex;
      SplitAddress(addr, l2_tag, l2_setIndex,2);
      SET & l2_set = _l2_sets[l2_setIndex];
      bool l2_hit = l2_set.Find(l2_tag);
      if ( (! l2_hit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
      {
        l2_set.Replace(l2_tag);
      }

      _l2_access[accessType][l2_hit]++;

      //bring data to l1 to maintain inclusiveness
      set.Replace(tag);

    }

    return hit; //need to modify
    */
}

// define shortcuts
#define CACHE_DIRECT_MAPPED(MAX_SETS, ALLOCATION) CACHE<CACHE_SET::DIRECT_MAPPED, MAX_SETS, ALLOCATION>
#define CACHE_LRU(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::LRU<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>

#endif // PIN_CACHE_H
