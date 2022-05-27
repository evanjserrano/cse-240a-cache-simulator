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
    icache = (tagstore**)calloc(icacheSets * icacheAssoc, sizeof(tagstore));
    dcache = (tagstore**)calloc(dcacheSets * dcacheAssoc, sizeof(tagstore));
    l2cache = (tagstore**)calloc(l2cacheSets * l2cacheAssoc, sizeof(tagstore));
}

void update_lru(tagstore* line, uint32_t assoc, uint32_t tag, uint16_t lru_val)
{
    uint32_t i;
    uint8_t inserted = FALSE;
    for (i = 0; i < assoc; i++)
    {
        if (!line[i].valid)
        {
            if (!inserted)
            {
                line[i].lru = 0;
                line[i].valid = TRUE;
                line[i].tag = tag;
                inserted = TRUE;
            }
        }
        // cascade other lru values down
        else if (line[i].lru < lru_val)
        {
            line[i].lru++;
        }
        // update new entry's tag and lru
        else if (line[i].lru == lru_val && !inserted)
        {
            line[i].lru = 0;
            line[i].tag = tag;
            inserted = TRUE;
        }
    }
}

// Perform a memory access through the icache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t icache_access(uint32_t addr)
{
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
    uint32_t i;
    // look for tag
    for (i = 0; i < icacheSets; i++)
    {
        tagstore t = cacheline[i];
        // cache hit
        if (t.valid && t.tag == tag)
        {
            // update lru
            update_lru(cacheline, icacheAssoc, tag, t.lru);
            return icacheHitTime;
        }
    }

    // cache miss
    uint32_t penalty = l2cache_access(addr);
    icachePenalties += penalty;
    icacheMisses++;
    // update lru
    // icacheAssoc - 1 is the least recently used
    update_lru(cacheline, icacheAssoc, tag, icacheAssoc - 1);
    return icacheHitTime + penalty;
}

uint32_t get_correct_entry(tagstore* cacheline, uint32_t assoc, uint32_t tag)
{
    uint32_t i;
    for (i = 0; i < assoc; i++)
    {
        tagstore entry = cacheline[i];
        if (entry.valid && entry.tag == tag)
        {
            return i;
        }
    }
    for (i = 0; i < assoc; i++)
    {
        tagstore entry = cacheline[i];
        if (!entry.valid)
        {
            return i;
        }
    }
    for (i = 0; i < assoc; i++)
    {
        tagstore entry = cacheline[i];
        if (entry.lru == assoc - 1)
        {
            return i;
        }
    }
    puts("ERR: SHOULDN'T BE HERE");
    return 0;
}

void set_entry(tagstore* cacheline, uint32_t assoc, uint32_t idx, uint32_t tag)
{
    uint16_t lru = cacheline[idx].lru;
    uint32_t i;
    if (lru == assoc - 1)
        for (i = 0; i < assoc; i++)
        {
            cacheline[i].lru++;
        }
    else if (!cacheline[idx].valid)
        for (i = 0; i < assoc; i++)
        {
            if (cacheline[i].valid)
                cacheline[i].lru++;
        }
    else
        for (i = 0; i < assoc; i++)
        {
            if (cacheline[i].valid && cacheline[i].lru < lru)
                cacheline[i].lru++;
        }

    cacheline[idx].lru = 0;
    cacheline[idx].valid = 1;
    cacheline[idx].tag = tag;
}

// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t dcache_access(uint32_t addr)
{
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

    if (cacheline[idx].tag != tag)
        dcacheMisses++;

    // cache + tag + idx. enters into cache and updates lru
    set_entry(cacheline, dcacheAssoc, idx, tag);

    return memspeed;
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t l2cache_access(uint32_t addr)
{
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
    uint32_t i;
    // look for tag
    for (i = 0; i < l2cacheSets; i++)
    {
        tagstore t = cacheline[i];
        // cache hit
        if (t.valid && t.tag == tag)
        {
            // update lru
            update_lru(cacheline, l2cacheAssoc, tag, t.lru);
            return l2cacheHitTime;
        }
    }

    // cache miss
    l2cachePenalties += memspeed;
    l2cacheMisses++;
    // update lru
    // l2cacheAssoc - 1 is the least recently used
    update_lru(cacheline, l2cacheAssoc, tag, l2cacheAssoc - 1);
    return l2cacheHitTime + memspeed;
}

void print_l2cache()
{
    // number of bits for each part
    uint8_t block_bits = log2(blocksize);
    uint8_t id_bits = log2(dcacheSets);
    uint8_t tag_bits = ADDR_SIZE - id_bits - block_bits;

    for (int id = 0; id < dcacheSets; id++)
    {
        tagstore* tag = CACHEIDX(dcache, id);
        printf("%#04x:\t", id);
        for (int t = 0; t < dcacheAssoc; t++)
        {
            printf("%01d,%#06x,%01d\t", tag[t].valid, tag[t].tag, tag[t].lru);
        }
        printf("\n");
    }
}