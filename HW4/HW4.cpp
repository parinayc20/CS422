/*
 * Copyright (C) 2007-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <iomanip>
#include <limits.h>
#include <ctime>


using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::set;
using std::map;
using std::left;
using std::setw;
using std::setfill;

#define printer(out, ins, count, percent) 			        \
{ 							                                \
    *out << left << setw(25) << setfill(' ') << ins;	    \
    *out << left << setw(25) << setfill(' ') << count; 	    \
    *out << left << setw(25) << setfill(' ') << percent; 	\
    *out << endl; 					                        \
}

#define tab_print(val) (*out << left << setw(25) << setfill(' ') << val)

/* ================================================================== */
// Global variables
/* ================================================================== */

#define L1_SIZE 128
#define L1_WAYS 8
#define L2_SIZE 1024
#define L2_WAYS 16

typedef struct cache_entry
{
    ADDRINT tag;
    UINT64 lru_num;
    BOOL valid;
    UINT64 hits;
} CACHE_ENTRY;

typedef struct srrip_entry 
{
    ADDRINT tag;
    UINT32 age;
    BOOL valid;
} SRRIP_ENTRY;

typedef struct nru_entry
{
    ADDRINT tag;
    BOOL ref;
    BOOL valid;
} NRU_ENTRY;

static UINT64 ff_cnt = 0;
static UINT64 FF_MUL = 1000000000;
static UINT64 instrument_cnt = 1000000000;
static UINT64 icount = 0;
static UINT64 pre_icount = 0;

static vector<vector<CACHE_ENTRY*>> L1;
static vector<vector<CACHE_ENTRY*>> L2;
static vector<vector<CACHE_ENTRY*>> L1_srrip;
static vector<vector<SRRIP_ENTRY*>> L2_srrip;
static vector<vector<CACHE_ENTRY*>> L1_nru;
static vector<vector<NRU_ENTRY*>> L2_nru;
static vector<UINT32> nru_cnt(L2_SIZE, 0);

static UINT64 L1_lru_num = 1;
static UINT64 L1_srrip_lru_num = 1;
static UINT64 L1_nru_lru_num = 1;
static UINT64 L2_lru_num = 1;

static UINT64 L1_access = 0;
static UINT64 L2_access = 0;
static UINT64 L1_miss = 0;
static UINT64 L2_miss = 0;
static vector<UINT64> L2_hits(3, 0);
static UINT64 L1_srrip_miss = 0;
static UINT64 L2_srrip_miss = 0;
static UINT64 L1_nru_miss = 0;
static UINT64 L2_nru_miss = 0;

std::ostream* out = &cerr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "result.out", 
                                "specify file name for MyPinTool output");

KNOB< UINT64 > KnobFastForward(KNOB_MODE_WRITEONCE, "pintool", "f", "0",
                                "instruction count to fast forward");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool prints out the number of dynamically executed " << endl
         << "instructions, basic blocks and threads in the application." << endl
         << endl;

    // TODO: check what is this
    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

VOID InsCount(UINT32 c)
{
    pre_icount = icount; // TODO: also try keeping previous BB ins count rather
    icount += c;
    // *out << icount << endl;
}

INT32 FastForward(void) {
    return ((pre_icount >= ff_cnt) && (pre_icount < ff_cnt + instrument_cnt));
}

INT32 Terminate(void) {
    return (icount >= ff_cnt + instrument_cnt);
}

VOID lru(ADDRINT memAddr, UINT64 size) {
    // *out << "lru" << endl;
    ADDRINT startAddrL1 = memAddr >> 6;
    ADDRINT endAddrL1 = (memAddr + size - 1) >> 6;

    for(ADDRINT addr = startAddrL1; addr <= endAddrL1; addr ++) {
        L1_access ++;
        ADDRINT idx = addr & (L1_SIZE - 1);
        ADDRINT tag = addr >> 7;
        bool found = false;
        for (int way = 0; way < L1_WAYS; way++) {
            if (L1[idx][way] -> valid && tag == L1[idx][way] -> tag){
                // L1 cache hit
                L1[idx][way] -> lru_num = L1_lru_num ++;
                found = true;
                break;
            }
        }

	// *out << "L1 done" << endl;
        if (found == false) {
            // L1 cache miss
            L1_miss ++;


            L2_access ++;
            ADDRINT idx_L2 = addr & (L2_SIZE - 1);
            ADDRINT tag_L2 = addr >> 10;
            bool found_L2 = false;

            for (int way = 0; way < L2_WAYS; way++) {
                if (L2[idx_L2][way] -> valid && tag_L2 == L2[idx_L2][way] -> tag) {
                    // L2 cache hit
                    L2[idx_L2][way] -> lru_num = L2_lru_num++;
                    L2[idx_L2][way] -> hits ++;
                    found_L2 = true;
                    break;
                }
            }

            if (found_L2 == false) {
                // L2 cache miss
                L2_miss ++;

                int min_way = 0;
                UINT64 min_lru = L2_lru_num;
                for (int way = 0; way < L2_WAYS; way++) {
                    if (L2[idx_L2][way] -> valid == false) {
                        min_way = way;
                        break;
                    }
                    if (L2[idx_L2][way] -> lru_num < min_lru) {
                        min_way = way;
                        min_lru = L2[idx_L2][way] -> lru_num;
                    }
                }

                if (L2[idx_L2][min_way] -> valid == true) {
                    if (L2[idx_L2][min_way] -> hits == 0)
                        L2_hits[0] ++;
                    if (L2[idx_L2][min_way] -> hits >= 1)
                        L2_hits[1] ++;
                    if (L2[idx_L2][min_way] -> hits >= 2)
                        L2_hits[2] ++;
                }

                if (L2[idx_L2][min_way] -> valid == true) {
                    // invalidate in L1 cache
                    ADDRINT addr_inv = (L2[idx_L2][min_way]->tag << 10) | idx_L2;
                    ADDRINT idx_inv = addr_inv & (L1_SIZE - 1);
                    ADDRINT tag_inv = addr_inv >> 7;

                    for(int way = 0; way < L1_WAYS; way++) {
                        if (tag_inv == L1[idx_inv][way] -> tag) {
                            L1[idx_inv][way] -> valid = false;
                        }
                    }
                }
                L2[idx_L2][min_way] -> valid = true;
                L2[idx_L2][min_way] -> tag = tag_L2;
                L2[idx_L2][min_way] -> lru_num = L2_lru_num++;
                L2[idx_L2][min_way] -> hits = 0;
            }

            int min_way = 0;
            UINT64 min_lru = L1_lru_num;
            for (int way = 0; way < L1_WAYS; way++) {
                if (L1[idx][way] -> valid == false) {
                    min_way = way;
                    break;
                }
                if (L1[idx][way] -> lru_num < min_lru) {
                    min_way = way;
                    min_lru = L1[idx][way] -> lru_num;
                }
            }
            L1[idx][min_way] -> valid = true;
            L1[idx][min_way] -> tag = tag;
            L1[idx][min_way] -> lru_num = L1_lru_num ++;
        }
    }

}

void srrip(ADDRINT memAddr, UINT64 size) {
    // *out << "srrip" << endl;
    ADDRINT startAddrL1 = memAddr >> 6;
    ADDRINT endAddrL1 = (memAddr + size - 1) >> 6;

    for(ADDRINT addr = startAddrL1; addr <= endAddrL1; addr ++) {
        ADDRINT idx = addr & (L1_SIZE - 1);
        ADDRINT tag = addr >> 7;
        bool found = false;
        for (int way = 0; way < L1_WAYS; way++) {
            if (L1_srrip[idx][way] -> valid && tag == L1_srrip[idx][way] -> tag){
                // L1_srrip cache hit
                L1_srrip[idx][way] -> lru_num = L1_srrip_lru_num ++;
                found = true;
                break;
            }
        }
	// *out << "L1 done" << endl;
        if (found == false) {
            // L1_srrip cache miss
            L1_srrip_miss++;

            ADDRINT idx_L2 = addr & (L2_SIZE - 1);
            ADDRINT tag_L2 = addr >> 10;
            bool found_L2 = false;

            for(int way = 0; way < L2_WAYS; way++) {
                if(L2_srrip[idx_L2][way] -> valid && L2_srrip[idx_L2][way] -> tag == tag_L2) {
                    // L2 cache hit
                    L2_srrip[idx_L2][way] -> age = 0;
                    found_L2 = true;
                    break;
                }
            }

            if (found_L2 == false) {
                // L2 cache miss
                L2_srrip_miss ++;

                UINT32 max_age = 0;
                int max_way = 0;

                for(int way = 0; way < L2_WAYS; way ++) {
                    if (L2_srrip[idx_L2][way] -> valid == false) {
                        max_way = way;
                        max_age = 3;
                        break;
                    }
                    if (L2_srrip[idx_L2][way] -> age > max_age) {
                        max_age = L2_srrip[idx_L2][way] -> age;
                        max_way = way;
                    }
                }

                for(int way = 0; way < L2_WAYS; way ++) {
                    if (L2_srrip[idx_L2][way] -> valid) {
                        L2_srrip[idx_L2][way]->age += (3 - max_age);
                    }
                }

                if (L2_srrip[idx_L2][max_way] -> valid == true) {
                    // invalidate in L1 cache
                    ADDRINT addr_inv = (L2_srrip[idx_L2][max_way]->tag << 10) | idx_L2;
                    ADDRINT idx_inv = addr_inv & (L1_SIZE - 1);
                    ADDRINT tag_inv = addr_inv >> 7;

                    for(int way = 0; way < L1_WAYS; way++) {
                        if (tag_inv == L1_srrip[idx_inv][way] -> tag) {
                            L1_srrip[idx_inv][way] -> valid = false;
                        }
                    }
                }
                L2_srrip[idx_L2][max_way] -> valid = true;
                L2_srrip[idx_L2][max_way] -> tag = tag_L2;
                L2_srrip[idx_L2][max_way] -> age = 2;
            }

            int min_way = 0;
            UINT64 min_lru = L1_srrip_lru_num;
            for (int way = 0; way < L1_WAYS; way++) {
                if (L1_srrip[idx][way] -> valid == false) {
                    min_way = way;
                    break;
                }
                if (L1_srrip[idx][way] -> lru_num < min_lru) {
                    min_way = way;
                    min_lru = L1_srrip[idx][way] -> lru_num;
                }
            }
            L1_srrip[idx][min_way] -> valid = true;
            L1_srrip[idx][min_way] -> tag = tag;
            L1_srrip[idx][min_way] -> lru_num = L1_srrip_lru_num ++;
        }
    }
    
}

void nru(ADDRINT memAddr, UINT64 size) {
    // *out << "nru" << endl;
    ADDRINT startAddrL1 = memAddr >> 6;
    ADDRINT endAddrL1 = (memAddr + size - 1) >> 6;

    for(ADDRINT addr = startAddrL1; addr <= endAddrL1; addr ++) {
        ADDRINT idx = addr & (L1_SIZE - 1);
        ADDRINT tag = addr >> 7;
        bool found = false;

        for (int way = 0; way < L1_WAYS; way++) {
            if (L1_nru[idx][way] -> valid && tag == L1_nru[idx][way] -> tag){
                // L1_nru cache hit
                L1_nru[idx][way] -> lru_num = L1_nru_lru_num ++;
                found = true;
                break;
            }
        }

	// *out << "L1 done" << endl;

        if (found == false) {
            // L1_nru cache miss
            L1_nru_miss ++;

            ADDRINT idx_L2 = addr & (L2_SIZE - 1);
            ADDRINT tag_L2 = addr >> 10;
            bool found_L2 = false;

            for (int way = 0; way < L2_WAYS; way++) {
                if (L2_nru[idx_L2][way] -> valid && L2_nru[idx_L2][way] -> tag == tag_L2) {
                    // L2 cache hit
                    if (nru_cnt[idx_L2] == (L2_WAYS - 1) && L2_nru[idx_L2][way]->ref == false)
                    {
                        for (int j=0; j < L2_WAYS; j++)
                            L2_nru[idx_L2][j] -> ref = false;
                        nru_cnt[idx_L2] = 0;
                    }
                    if (L2_nru[idx_L2][way]->ref == false)
                        nru_cnt[idx_L2] ++;
                    L2_nru[idx_L2][way]->ref = true;
                    found_L2 = true;
                    break;
                }
            }

            if(found_L2 == false) {
                // L2 cache miss
                L2_nru_miss ++;

                int min_way = 0;
                for (int way = 0; way < L2_WAYS; way++) {
                    if (L2_nru[idx_L2][way] -> valid == false) {
                        min_way = way;
                        break;
                    }
                    if (L2_nru[idx_L2][way] -> ref == false) {
                        min_way = way;
                        break;
                    }
                }

                if (nru_cnt[idx_L2] == (L2_WAYS - 1)) {
                    for (int j=0; j < L2_WAYS; j++)
                        L2_nru[idx_L2][j] -> ref = false;
                    nru_cnt[idx_L2] = 0;
                }

                if (L2_nru[idx_L2][min_way] -> valid == true) {
                    // invalidate in L1 cache
                    ADDRINT addr_inv = (L2_nru[idx_L2][min_way]->tag << 10) | idx_L2;
                    ADDRINT idx_inv = addr_inv & (L1_SIZE - 1);
                    ADDRINT tag_inv = addr_inv >> 7;

                    for(int way = 0; way < L1_WAYS; way++) {
                        if (tag_inv == L1_nru[idx_inv][way] -> tag) {
                            L1_nru[idx_inv][way] -> valid = false;
                        }
                    }
                }

                nru_cnt[idx_L2] ++;
                L2_nru[idx_L2][min_way] -> ref = true;
                L2_nru[idx_L2][min_way] -> valid = true;
                L2_nru[idx_L2][min_way] -> tag = tag_L2;
            }

            int min_way = 0;
            UINT64 min_lru = L1_nru_lru_num;
            for (int way = 0; way < L1_WAYS; way++) {
                if (L1_nru[idx][way] -> valid == false) {
                    min_way = way;
                    break;
                }
                if (L1_nru[idx][way] -> lru_num < min_lru) {
                    min_way = way;
                    min_lru = L1_nru[idx][way] -> lru_num;
                }
            }
            L1_nru[idx][min_way] -> valid = true;
            L1_nru[idx][min_way] -> tag = tag;
            L1_nru[idx][min_way] -> lru_num = L1_nru_lru_num ++;
        }
    }

}


VOID Exit() {
    *out << "LRU stats: " << endl;

    tab_print("L1 accesses");
    tab_print("L2 accesses");
    tab_print("L1 misses");
    tab_print("L2 misses");
    tab_print("Dead-on-fill (%)");
    tab_print("2 hits (%)");
    *out << endl;

    if (L2_hits[1] == 0 && L2_hits[2] == 0)
        L2_hits[1] = 1;

    tab_print(L1_access);
    tab_print(L2_access);
    tab_print(L1_miss);
    tab_print(L2_miss);
    tab_print((L2_hits[0] * 100.0) / L2_miss);
    tab_print((L2_hits[2] * 100.0) / L2_hits[1]);
    *out << endl;
    *out << endl;

    *out << "SRRIP stats:" << endl;
    tab_print("L1 misses");
    tab_print("L2 misses");
    *out << endl;
    tab_print(L1_srrip_miss);
    tab_print(L2_srrip_miss);
    *out << endl;
    *out << endl;

    *out << "NRU stats:" << endl;
    tab_print("L1 misses");
    tab_print("L2 misses");
    *out << endl;
    tab_print(L1_nru_miss);
    tab_print(L2_nru_miss);
    *out << endl;
    *out << endl;

    exit(0);
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

VOID Instruction(INS ins, VOID* v) 
{
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
    // prefixed instructions appear as predicated instructions in Pin.
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)Exit, IARG_END);

    UINT32 memOperands = INS_MemoryOperandCount(ins);

    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        UINT64 size = INS_MemoryOperandSize(ins, memOp);

        if (INS_MemoryOperandIsRead(ins, memOp)) {
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)lru,
                IARG_MEMORYOP_EA, memOp,
                IARG_UINT64, size,
                IARG_END);

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)srrip,
                IARG_MEMORYOP_EA, memOp,
                IARG_UINT64, size,
                IARG_END);

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)nru,
                IARG_MEMORYOP_EA, memOp,
                IARG_UINT64, size,
                IARG_END);
        }

        if (INS_MemoryOperandIsWritten(ins, memOp)) {
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)lru,
                IARG_MEMORYOP_EA, memOp,
                IARG_UINT64, size,
                IARG_END);

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)srrip,
                IARG_MEMORYOP_EA, memOp,
                IARG_UINT64, size,
                IARG_END);

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)nru,
                IARG_MEMORYOP_EA, memOp,
                IARG_UINT64, size,
                IARG_END);
        }
        
    }

    UINT32 num_ins = 1;
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InsCount, IARG_UINT32, num_ins, IARG_END);

}

/*!
 * Insert call to the CountBbl() analysis routine before every basic block 
 * of the trace.
 * This function is called every time a new trace is encountered.
 * @param[in]   trace    trace to be instrumented
 * @param[in]   v        value specified by the tool in the TRACE_AddInstrumentFunction
 *                       function call
 */
// VOID Trace(TRACE trace, VOID* v)
// {
//     // Visit every basic block in the trace
//     for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
//     {
//         BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
//         BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)Exit, IARG_END);

//         // Insert a call to CountBbl() before every basic bloc, passing the number of instructions
//         BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)InsCount, IARG_UINT32, BBL_NumIns(bbl), IARG_END);

//         BOOL break_flag = 1;
//         for (INS ins = BBL_InsHead(bbl); break_flag && INS_Valid(ins); ins = INS_Next(ins)) 
//         {
//             break_flag = (ins != BBL_InsTail(bbl));
//             Instruction(ins);
//         }
//     }
// }

// TODO: there was an instruction to increase the number of threads. See if useful

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID* v)
{
    *out << "Finished Binary" << endl;
    *out << "Instruction number: " << icount << endl;
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments, 
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char* argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    string fileName = KnobOutputFile.Value();

    if (!fileName.empty())
    {
        out = new std::ofstream(fileName.c_str());
    }

    ff_cnt = KnobFastForward * FF_MUL;


    cerr << "Fast Forward amount :" << ff_cnt << endl;
    cerr << "Output File name :" << fileName << endl;
    cerr << "Cutoff Point :" << ff_cnt + instrument_cnt << endl;

    for (int i=0; i < L1_SIZE; i++){
        vector<CACHE_ENTRY*> v;
        vector<CACHE_ENTRY*> v_srrip;
        vector<CACHE_ENTRY*> v_nru;

        for (int j=0 ; j < L1_WAYS; j++) {
            CACHE_ENTRY* entry = new CACHE_ENTRY;
            entry -> valid = false;
            entry -> lru_num = 0;
            v.push_back(entry);

            CACHE_ENTRY* entry_srrip = new CACHE_ENTRY;
            entry_srrip -> valid = false;
            entry_srrip -> lru_num = 0;
            v_srrip.push_back(entry_srrip);

            CACHE_ENTRY* entry_nru = new CACHE_ENTRY;
            entry_nru -> valid = false;
            entry_nru -> lru_num = 0;
            v_nru.push_back(entry_nru);
        }
        L1.push_back(v);
        L1_srrip.push_back(v_srrip);
        L1_nru.push_back(v_nru);
    }

    for (int i=0; i < L2_SIZE; i++){
        vector<CACHE_ENTRY*> v;
        vector<SRRIP_ENTRY*> v_srrip;
        vector<NRU_ENTRY*> v_nru;

        for (int j=0 ; j < L2_WAYS; j++) {
            CACHE_ENTRY* entry = new CACHE_ENTRY;
            entry -> valid = false;
            entry -> lru_num = 0;
            entry -> hits = 0;
            v.push_back(entry);

            SRRIP_ENTRY* srrip_entry = new SRRIP_ENTRY;
            srrip_entry -> valid = false;
            v_srrip.push_back(srrip_entry);

            NRU_ENTRY* nru_entry = new NRU_ENTRY;
            nru_entry -> valid = false;
            v_nru.push_back(nru_entry);
        }
        L2.push_back(v);
        L2_srrip.push_back(v_srrip);
        L2_nru.push_back(v_nru);
    }

    // Register function to be called to instrument traces
    INS_AddInstrumentFunction(Instruction, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    cerr << "===============================================" << endl;
    cerr << "This application is instrumented by HW1" << endl;
    if (!KnobOutputFile.Value().empty())
    {
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    }
    cerr << "===============================================" << endl;

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
