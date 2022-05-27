//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include "cache.h"
#include <math.h>
#include <stdio.h>

//
// Student Information
//
const char* studentName = "Evan Serrano";
const char* studentID = "A15543204";
const char* email = "e1serran@ucsd.edu";

//------------------------------------//
//        Cache Configuration         //
//------------------------------------//

uint32_t icacheSets;    // Number of sets in the I$
uint32_t icacheAssoc;   // Associativity of the I$
uint32_t icacheHitTime; // Hit Time of the I$

uint32_t dcacheSets;    // Number of sets in the D$
uint32_t dcacheAssoc;   // Associativity of the D$
uint32_t dcacheHitTime; // Hit Time of the D$

uint32_t l2cacheSets;    // Number of sets in the L2$
uint32_t l2cacheAssoc;   // Associativity of the L2$
uint32_t l2cacheHitTime; // Hit Time of the L2$
uint32_t inclusive;      // Indicates if the L2 is inclusive

uint32_t blocksize; // Block/Line size
uint32_t memspeed;  // Latency of Main Memory

//------------------------------------//
//          Cache Statistics          //
//------------------------------------//

uint64_t icacheRefs;      // I$ references
uint64_t icacheMisses;    // I$ misses
uint64_t icachePenalties; // I$ penalties

uint64_t dcacheRefs;      // D$ references
uint64_t dcacheMisses;    // D$ misses
uint64_t dcachePenalties; // D$ penalties

uint64_t l2cacheRefs;      // L2$ references
uint64_t l2cacheMisses;    // L2$ misses
uint64_t l2cachePenalties; // L2$ penalties

//------------------------------------//
//        Cache Data Structures       //
//------------------------------------//

typedef struct
{
    uint32_t tag;
    uint16_t lru;
    uint8_t valid;
} tagstore;

#define ADDR_SIZE 32
#define CACHEIDX(cache, id)                                                    \
    (tagstore*)(cache +                                                        \
                (id * cache##Assoc * sizeof(tagstore) / sizeof(tagstore*)))

tagstore** icache;
tagstore** dcache;
tagstore** l2cache;

//------------------------------------//
//          Cache Functions           //
//------------------------------------//

// Initialize the Cache Hierarchy
//
void init_cache()
{
    // Initialize cache stats
    icacheRefs = 0;
    icacheMisses = 0;
    icachePenalties = 0;
    dcacheRefs = 0;
    dcacheMisses = 0;
    dcachePenalties = 0;
    l2cacheRefs = 0;
    l2cacheMisses = 0;
    l2cachePenalties = 0;

    // Initialize Cache Simulator Data Structures
    if (icacheSets == 0 || icacheAssoc == 0)
        icache = NULL;
    else
        icache = (tagstore**)calloc(icacheSets * icacheAssoc, sizeof(tagstore));
    if (dcacheSets == 0 || dcacheAssoc == 0)
        dcache = NULL;
    else
        dcache = (tagstore**)calloc(dcacheSets * dcacheAssoc, sizeof(tagstore));
    if (l2cacheSets == 0 || l2cacheAssoc == 0)
        l2cache = NULL;
    else
        l2cache =
            (tagstore**)calloc(l2cacheSets * l2cacheAssoc, sizeof(tagstore));
}

/**
 * @brief Get the correct entry of the cache if it exists. Otherwise, get the
 * location it should be inserted.
 *
 * @param cacheline pointer to list of cache blocks (tagstores)
 * @param assoc number of tagstores in the cacheline
 * @param tag tag to look for
 * @return uint32_t of the index in cacheline where entry should be
 */
uint32_t get_correct_entry(tagstore* cacheline, uint32_t assoc, uint32_t tag)
{
    uint32_t i;
    uint32_t the_lru; // the true least recently used
    for (i = 0; i < assoc; i++)
    {
        tagstore entry = cacheline[i];

        if (!entry.valid || entry.tag == tag)
        {
            return i;
        }
        if (entry.lru == assoc - 1)
        {
            the_lru = i;
        }
    }
    return the_lru;
}

/**
 * @brief Set the entry in the cacheline based on an index. Also update the lru
 * values.
 *
 * @param cacheline pointer to list of cache tagstores
 * @param assoc number of tagstores in the cacheline
 * @param idx index of cacheline where tag should be stored
 * @param tag tag to look for
 */
void set_entry(tagstore* cacheline, uint32_t assoc, uint32_t idx, uint32_t tag)
{
    uint16_t lru = cacheline[idx].lru;
    uint32_t i;
    for (i = 0; i < assoc; i++)
    {
        if (cacheline[i].valid &&
            (!cacheline[idx].valid || cacheline[i].lru < lru))
        {
            cacheline[i].lru++;
        }
    }

    cacheline[idx].lru = 0;
    cacheline[idx].valid = 1;
    cacheline[idx].tag = tag;
}

// Perform a memory access through the icache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t icache_access(uint32_t addr)
{
    if (icache == NULL)
        return memspeed;
    icacheRefs++;

    // number of bits for each part
    uint8_t block_bits = log2(blocksize);
    uint8_t id_bits = log2(icacheSets);
    uint8_t tag_bits = ADDR_SIZE - id_bits - block_bits;

    // actual addr part for each part
    uint32_t block_offset = addr & (blocksize - 1);
    uint32_t id = (addr >> block_bits) & (icacheSets - 1);
    uint32_t tag = (addr >> (block_bits + id_bits) & ((1 << tag_bits) - 1));

    tagstore* cacheline = CACHEIDX(icache, id);

    // cache + tag --> entry idx of block placement
    uint32_t idx = get_correct_entry(cacheline, icacheAssoc, tag);

    uint32_t penalty = 0;
    // cache miss
    if (!cacheline[idx].valid || cacheline[idx].tag != tag)
    {
        icacheMisses++;
        icachePenalties += (penalty = l2cache_access(addr));
    }

    // cache + tag + idx. enters into cache and updates lru
    set_entry(cacheline, icacheAssoc, idx, tag);

    return icacheHitTime + penalty;
}

// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t dcache_access(uint32_t addr)
{
    if (dcache == NULL)
        return memspeed;
    dcacheRefs++;

    // number of bits for each part
    uint8_t block_bits = log2(blocksize);
    uint8_t id_bits = log2(dcacheSets);
    uint8_t tag_bits = ADDR_SIZE - id_bits - block_bits;

    // actual addr part for each part
    uint32_t block_offset = addr & (blocksize - 1);
    uint32_t id = (addr >> block_bits) & (dcacheSets - 1);
    uint32_t tag = (addr >> (block_bits + id_bits) & ((1 << tag_bits) - 1));

    tagstore* cacheline = CACHEIDX(dcache, id);

    // cache + tag --> entry idx of block placement
    uint32_t idx = get_correct_entry(cacheline, dcacheAssoc, tag);

    uint32_t penalty = 0;
    // cache miss
    if (!cacheline[idx].valid || cacheline[idx].tag != tag)
    {
        dcacheMisses++;
        dcachePenalties += (penalty = l2cache_access(addr));
    }

    // cache + tag + idx. enters into cache and updates lru
    set_entry(cacheline, dcacheAssoc, idx, tag);

    return dcacheHitTime + penalty;
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t l2cache_access(uint32_t addr)
{
    if (l2cache == NULL)
        return memspeed;
    l2cacheRefs++;

    // number of bits for each part
    uint8_t block_bits = log2(blocksize);
    uint8_t id_bits = log2(l2cacheSets);
    uint8_t tag_bits = ADDR_SIZE - id_bits - block_bits;

    // actual addr part for each part
    uint32_t block_offset = addr & (blocksize - 1);
    uint32_t id = (addr >> block_bits) & (l2cacheSets - 1);
    uint32_t tag = (addr >> (block_bits + id_bits) & ((1 << tag_bits) - 1));

    tagstore* cacheline = CACHEIDX(l2cache, id);

    // cache + tag --> entry idx of block placement
    uint32_t idx = get_correct_entry(cacheline, l2cacheAssoc, tag);

    uint32_t penalty = 0;
    // cache miss
    if (!cacheline[idx].valid || cacheline[idx].tag != tag)
    {
        l2cacheMisses++;
        l2cachePenalties += (penalty = memspeed);
    }

    // cache + tag + idx. enters into cache and updates lru
    set_entry(cacheline, l2cacheAssoc, idx, tag);

    return l2cacheHitTime + penalty;
}