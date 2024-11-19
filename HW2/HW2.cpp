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

#define MASK_512 511
#define MASK_1024 1023
#define MASK_128 127

#define BTB_SETS 128
#define BTB_WAYS 4

#define printer(out, strat, tcount, tfrac, ffrac, bfrac)                                    \
{ 							                                                                \
    *out << left << setw(35) << setfill(' ') << strat;	                                    \
    *out << left << setw(35) << setfill(' ') << tcount;	                                    \
    *out << left << setw(35) << setfill(' ') << tfrac;	                                    \
    *out << left << setw(35) << setfill(' ') << ffrac; 	                                    \
    *out << left << setw(35) << setfill(' ') << bfrac; 	                                    \
    *out << endl; 					                                                        \
}

#define printer2(out, strat, tcount, tfrac, misfrac)                                        \
{ 							                                                                \
    *out << left << setw(35) << setfill(' ') << strat;	                                    \
    *out << left << setw(35) << setfill(' ') << tcount;	                                    \
    *out << left << setw(35) << setfill(' ') << tfrac;	                                    \
    *out << left << setw(35) << setfill(' ') << misfrac;	                                \
    *out << endl; 					                                                        \
}

/* ================================================================== */
// Global variables
/* ================================================================== */

enum Predictor 
{
    fnbt, 
    bimodal,
    sag,
    gag,
    gshare,
    hybrid1,
    hybrid2_majority,
    hybrid2_tournament,

    predictors
};

static vector<string> bpreds = {"FNBT", "Bimodal", "SAg", "GAg", "gshare",
                                "Hybrid-1", "Hybrid-2 Majority", "Hybrid-2 Tournament"};

enum Direction
{
    forward,
    backward,

    directions
};

typedef struct BtbEntry 
{
    BOOL valid;
    ADDRINT tag;
    UINT64 lru_state;
    ADDRINT target;
} BTB_ENTRY;


static UINT64 ff_cnt = 0;
static UINT64 FF_MUL = 1000000000;
static UINT64 instrument_cnt = 1000000000;
static UINT64 icount = 0;
static UINT64 pre_icount = 0;

static vector<vector<UINT64>> hits;
static vector<vector<UINT64>> misses;

static vector<UINT32> bimod_pht(512, 0);
static vector<UINT32> sag_bht(1024, 0);
static vector<UINT32> sag_pht(512, 0);
static UINT32 ghr = 0;
static vector<UINT32> gag_pht(512, 0);
static vector<UINT32> gshare_pht(512, 0);
static vector<UINT32> meta_gag_sag(512, 0);
static vector<UINT32> meta_gag_gshare(512, 0);
static vector<UINT32> meta_gshare_sag(512, 0);

static UINT64 btb_preds = 0;
static UINT64 btb_fails = 0;
static UINT64 btb_misses = 0;
static UINT64 btb2_preds = 0;
static UINT64 btb2_fails = 0;
static UINT64 btb2_misses = 0;
static UINT32 btb2_ghr = 0;
static vector<vector<BTB_ENTRY*>> btb;
static vector<vector<BTB_ENTRY*>> btb2;
static UINT64 lru_num = 1;

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

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

VOID InsCount(UINT32 c)
{
    pre_icount = icount; 
    icount += c;
}

INT32 FastForward(void) {
    return ((pre_icount >= ff_cnt) && (pre_icount < ff_cnt + instrument_cnt));
}

INT32 Terminate(void) {
    return (icount >= ff_cnt + instrument_cnt);
}

VOID Fnbt(ADDRINT InsAddr, ADDRINT BranchAddr, BOOL taken) {
    if (InsAddr < BranchAddr) {
        hits[fnbt][forward] += (taken == false);
        misses[fnbt][forward] += (taken == true);
    }
    else {
        hits[fnbt][backward] += (taken == true);
        misses[fnbt][backward] += (taken == false);
    }
}

VOID predict(ADDRINT InsAddr, ADDRINT BranchAddr, BOOL taken) {
    UINT32 direction = BranchAddr > InsAddr ? forward : backward;

    UINT32 pc = InsAddr & MASK_512; // TODO: if this is how the XOR is to be done (being used in hy1 also)
    UINT32 bht_ind = InsAddr & MASK_1024;

    BOOL pred_bomid = bimod_pht[InsAddr & MASK_512] > 1;
    BOOL pred_sag = sag_pht[sag_bht[bht_ind]] > 1;
    BOOL pred_gag = gag_pht[ghr] > 3;
    BOOL pred_gshare = gshare_pht[ghr ^ pc] > 3;
    BOOL pred_hy1 = meta_gag_sag[ghr] > 1 ? pred_gag : pred_sag; 
    BOOL pred_hy2_majority = ((UINT32)pred_sag + (UINT32)pred_gag + (UINT32)pred_gshare) > 1;

    BOOL pred_hy2_tournament = 0;
    if (meta_gag_sag[ghr] > 1) 
        pred_hy2_tournament = meta_gag_gshare[ghr] > 1 ? pred_gag : pred_gshare;
    else
        pred_hy2_tournament = meta_gshare_sag[ghr] > 1 ? pred_gshare : pred_sag;

    if (taken) {
        hits[bimodal][direction] += (pred_bomid == true);
        misses[bimodal][direction] += (pred_bomid == false);
        if (bimod_pht[InsAddr & MASK_512] < 3)
            bimod_pht[InsAddr & MASK_512] ++;

        hits[sag][direction] += (pred_sag == true);
        misses[sag][direction] += (pred_sag == false);
        if (sag_pht[sag_bht[bht_ind]] < 3) 
            sag_pht[sag_bht[bht_ind]] ++;
        sag_bht[bht_ind] = ((sag_bht[bht_ind] << 1) | 1) & MASK_512;

        hits[gag][direction] += (pred_gag == true);
        misses[gag][direction] += (pred_gag == false);
        if (gag_pht[ghr] < 7)
            gag_pht[ghr] ++;

        hits[gshare][direction] += (pred_gshare == true);
        misses[gshare][direction] += (pred_gshare == false);
        if (gshare_pht[ghr ^ pc] < 7)
            gshare_pht[ghr ^ pc] ++;

        hits[hybrid1][direction] += (pred_hy1 == true);
        misses[hybrid1][direction] += (pred_hy1 == false);
        if (pred_sag != pred_gag) {
            if (pred_gag == true && meta_gag_sag[ghr] < 3)
                meta_gag_sag[ghr] ++;
            if (pred_sag == true && meta_gag_sag[ghr] > 0)
                meta_gag_sag[ghr] --;
        }

        hits[hybrid2_majority][direction] += (pred_hy2_majority == true);
        misses[hybrid2_majority][direction] += (pred_hy2_majority == false);

        hits[hybrid2_tournament][direction] += (pred_hy2_tournament == true);
        misses[hybrid2_tournament][direction] += (pred_hy2_tournament == false);
        if (pred_gshare != pred_gag) {
            if (pred_gag == true && meta_gag_gshare[ghr] < 3)
                meta_gag_gshare[ghr] ++;
            if (pred_gshare == true && meta_gag_gshare[ghr] > 0)
                meta_gag_gshare[ghr] --;
        }
        if (pred_sag != pred_gshare) {
            if (pred_gshare == true && meta_gshare_sag[ghr] < 3)
                meta_gshare_sag[ghr] ++;
            if (pred_sag == true && meta_gshare_sag[ghr] > 0)
                meta_gshare_sag[ghr] --;
        }

        ghr = ((ghr << 1) | 1) & MASK_512;
        btb2_ghr = ((btb2_ghr << 1) | 1) & MASK_128;
    }
    else {
        hits[bimodal][direction] += (pred_bomid == false);
        misses[bimodal][direction] += (pred_bomid == true);
        if (bimod_pht[InsAddr & MASK_512] > 0)
            bimod_pht[InsAddr & MASK_512] --;

        hits[sag][direction] += (pred_sag == false);
        misses[sag][direction] += (pred_sag == true);
        if (sag_pht[sag_bht[bht_ind]] > 0)
            sag_pht[sag_bht[bht_ind]] --;
        sag_bht[bht_ind] = ((sag_bht[bht_ind] << 1) | 0) & MASK_512;

        hits[gag][direction] += (pred_gag == false);
        misses[gag][direction] += (pred_gag == true);
        if(gag_pht[ghr] > 0)
            gag_pht[ghr] --;

        hits[gshare][direction] += (pred_gshare == false);
        misses[gshare][direction] += (pred_gshare == true);
        if (gshare_pht[ghr ^ pc] > 0)
            gshare_pht[ghr ^ pc] --;

        hits[hybrid1][direction] += (pred_hy1 == false);
        misses[hybrid1][direction] += (pred_hy1 == true);
        if (pred_sag != pred_gag) {
            if (pred_gag == false && meta_gag_sag[ghr] < 3)
                meta_gag_sag[ghr] ++;
            if (pred_sag == false && meta_gag_sag[ghr] > 0)
                meta_gag_sag[ghr] --;
        }

        hits[hybrid2_majority][direction] += (pred_hy2_majority == false);
        misses[hybrid2_majority][direction] += (pred_hy2_majority == true);

        hits[hybrid2_tournament][direction] += (pred_hy2_tournament == false);
        misses[hybrid2_tournament][direction] += (pred_hy2_tournament == true);
        if (pred_gshare != pred_gag) {
            if (pred_gag == false && meta_gag_gshare[ghr] < 3)
                meta_gag_gshare[ghr] ++;
            if (pred_gshare == false && meta_gag_gshare[ghr] > 0)
                meta_gag_gshare[ghr] --;
        }
        if (pred_sag != pred_gshare) {
            if (pred_gshare == false && meta_gshare_sag[ghr] < 3)
                meta_gshare_sag[ghr] ++;
            if (pred_sag == false && meta_gshare_sag[ghr] > 0)
                meta_gshare_sag[ghr] --;
        }

        ghr = ((ghr << 1) | 0) & MASK_512;
        btb2_ghr = ((btb2_ghr << 1) | 0) & MASK_128;
    }
}

VOID BtbFill(ADDRINT InsAddr, ADDRINT BranchAddr, BOOL taken, UINT32 InsSize) {
    UINT32 btb_ind = InsAddr & MASK_128;
    ADDRINT tag = InsAddr >> 7;
    ADDRINT next_ins = InsAddr + InsSize;
    BOOL found = false;
    ADDRINT target = next_ins;
    int col_ind = 0;

    for (int i = 0; i < BTB_WAYS; i++) {
        if(btb[btb_ind][i] -> valid == 1 && btb[btb_ind][i] -> tag == tag) {
            found = true;
            target = btb[btb_ind][i] -> target;
            col_ind = i;
            break;
        }
    }

    btb_preds++;

    // TODO: Won't it always be taken
    if (taken) {
        btb_fails += (BranchAddr != target);

        if (found && BranchAddr != target) 
            btb[btb_ind][col_ind] -> target = BranchAddr;

        if (found == false && BranchAddr != target) {
            int insert_col = 0;
            UINT32 lru = btb[btb_ind][0] -> lru_state;

            for (int i = 0; i < BTB_WAYS; i++) {
                if (btb[btb_ind][i] -> valid == 0) {
                    insert_col = i;
                    break;
                }
                if (lru > btb[btb_ind][i] -> lru_state) {
                    lru = btb[btb_ind][i] -> lru_state;
                    insert_col = i;
                }
            }

            btb[btb_ind][insert_col] -> valid = 1;
            btb[btb_ind][insert_col] -> tag = tag;
            btb[btb_ind][insert_col] -> target = BranchAddr;
            btb[btb_ind][insert_col] -> lru_state = lru_num ++;
        }

    }
    else {
        btb_fails += (target != next_ins);
        if (found)
            btb[btb_ind][col_ind] -> valid = 0;
    }

    if (found)
        btb[btb_ind][col_ind] -> lru_state = lru_num ++;
    else    
        btb_misses ++;
}

VOID Btb2Fill(ADDRINT InsAddr, ADDRINT BranchAddr, BOOL taken, UINT32 InsSize) {
    UINT32 btb_ind = (InsAddr & MASK_128) ^ btb2_ghr;
    ADDRINT tag = InsAddr;
    ADDRINT next_ins = InsAddr + InsSize;
    BOOL found = false;
    ADDRINT target = next_ins;
    int col_ind = 0;

    for (int i = 0; i < BTB_WAYS; i++) {
        if(btb2[btb_ind][i] -> valid == 1 && btb2[btb_ind][i] -> tag == tag) {
            found = true;
            target = btb2[btb_ind][i] -> target;
            col_ind = i;
            break;
        }
    }

    btb2_preds++;

    // TODO: Won't it always be taken
    if (taken) {
        btb2_fails += (BranchAddr != target);

        if (found && BranchAddr != target) 
            btb2[btb_ind][col_ind] -> target = BranchAddr;

        if (found == false && BranchAddr != target) {
            int insert_col = 0;
            UINT32 lru = btb2[btb_ind][0] -> lru_state;

            for (int i = 0; i < BTB_WAYS; i++) {
                if (btb2[btb_ind][i] -> valid == 0) {
                    insert_col = i;
                    break;
                }
                if (lru > btb2[btb_ind][i] -> lru_state) {
                    lru = btb2[btb_ind][i] -> lru_state;
                    insert_col = i;
                }
            }

            btb2[btb_ind][insert_col] -> valid = 1;
            btb2[btb_ind][insert_col] -> tag = tag;
            btb2[btb_ind][insert_col] -> target = BranchAddr;
            btb2[btb_ind][insert_col] -> lru_state = lru_num ++;
        }

    }
    else {
        btb2_fails += (target != next_ins);
        if (found)
            btb2[btb_ind][col_ind] -> valid = 0;
    }

    if (found)
        btb2[btb_ind][col_ind] -> lru_state = lru_num ++;
    else    
        btb2_misses ++;
}

VOID Exit() {
    printer(out, "Predictor", "Total Predictions", "Misprediction Fraction (%)", "Forward Misprediction Fraction (%)",
            "Backward Mispredcition Fraction (%)");

    *out << endl;

    for (int i = 0; i < predictors; i++) {
        UINT64 total_predictions = hits[i][0] + hits[i][1] + misses[i][0] + misses[i][1];
        UINT64 total_mispredicts = misses[i][0] + misses[i][1];
        float total_fraction = (total_mispredicts * 100.0) / total_predictions;

        UINT64 forward_predicts = hits[i][forward] + misses[i][forward];
        UINT64 forward_mispredicts = misses[i][forward];
        float forward_fraction = (forward_mispredicts * 100.0) / forward_predicts;

        UINT64 backward_predicts = hits[i][backward] + misses[i][backward];
        UINT64 backward_mispredicts = misses[i][backward];
        float backward_fraction = (backward_mispredicts * 100.0) / backward_predicts;

        printer(out, bpreds[i], total_predictions, total_fraction, forward_fraction, backward_fraction);
    }

    *out << endl;
    *out << endl;

    printer2(out, "BTB Type", "BTB Predictions", "BTB Misprediction Fraction", "BTB Miss Rate");

    *out << endl;

    float btb_mf = (btb_fails * 100.0) / btb_preds;
    float btb_mr = (btb_misses * 100.0) / btb_preds;
    float btb2_mf = (btb2_fails * 100.0) / btb2_preds;
    float btb2_mr = (btb2_misses * 100.0) / btb2_preds;

    printer2(out, "BTB A", btb_preds, btb_mf, btb_mr);
    printer2(out, "BTB B", btb2_preds, btb2_mf, btb2_mr);

    exit(0);
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */


// TODO: Find out what is xbegin and xend
VOID Instruction1(INS ins) 
{
    // QUESTION: Will these calls be predicated

    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenCall(
        ins, IPOINT_BEFORE, (AFUNPTR)Fnbt,
        IARG_INST_PTR,
        IARG_BRANCH_TARGET_ADDR ,
        IARG_BRANCH_TAKEN,
        IARG_END
    );

    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenCall(
        ins, IPOINT_BEFORE, (AFUNPTR)predict,
        IARG_INST_PTR,
        IARG_BRANCH_TARGET_ADDR ,
        IARG_BRANCH_TAKEN,
        IARG_END
    );

}

VOID Instruction2(INS ins)
{
    UINT32 ins_size = INS_Size(ins);

    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenCall(
        ins, IPOINT_BEFORE, (AFUNPTR)BtbFill,
        IARG_INST_PTR,
        IARG_BRANCH_TARGET_ADDR ,
        IARG_BRANCH_TAKEN,
        IARG_UINT32, ins_size,
        IARG_END
    );

    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenCall(
        ins, IPOINT_BEFORE, (AFUNPTR)Btb2Fill,
        IARG_INST_PTR,
        IARG_BRANCH_TARGET_ADDR ,
        IARG_BRANCH_TAKEN,
        IARG_UINT32, ins_size,
        IARG_END
    );
}

/*!
 * Insert call to the CountBbl() analysis routine before every basic block 
 * of the trace.
 * This function is called every time a new trace is encountered.
 * @param[in]   trace    trace to be instrumented
 * @param[in]   v        value specified by the tool in the TRACE_AddInstrumentFunction
 *                       function call
 */
VOID Trace(TRACE trace, VOID* v)
{
    // Visit every basic block in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)Exit, IARG_END);

        // Insert a call to CountBbl() before every basic bloc, passing the number of instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)InsCount, IARG_UINT32, BBL_NumIns(bbl), IARG_END);

        BOOL break_flag = 1;
        for (INS ins = BBL_InsHead(bbl); break_flag && INS_Valid(ins); ins = INS_Next(ins)) 
        {
            break_flag = (ins != BBL_InsTail(bbl));
            if (INS_IsBranch(ins) && INS_HasFallThrough(ins))
                Instruction1(ins);
            else if (INS_IsIndirectControlFlow(ins)) 
                Instruction2(ins);
        }
    }
}

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
    for (int i = 0; i < predictors; i++) {
        vector<UINT64> v1(2, 0);
        vector<UINT64> v2(2, 0);
        hits.push_back(v1);
        misses.push_back(v2);
    }

    for (int i = 0; i < BTB_SETS; i++) {
        vector<BTB_ENTRY*> v1;
        for (int j = 0; j < BTB_WAYS; j++) {
            BTB_ENTRY* entry = new BTB_ENTRY;
            entry -> valid = 0;
            entry -> tag = 0;
            entry -> lru_state = 0;
            entry -> target = 0;
            v1.push_back(entry);
        }
        btb.push_back(v1);
    }

     for (int i = 0; i < BTB_SETS; i++) {
        vector<BTB_ENTRY*> v1;
        for (int j = 0; j < BTB_WAYS; j++) {
            BTB_ENTRY* entry = new BTB_ENTRY;
            entry -> valid = 0;
            entry -> tag = 0;
            entry -> lru_state = 0;
            entry -> target = 0;
            v1.push_back(entry);
        }
        btb2.push_back(v1);
    }


    cerr << "Fast Forward amount :" << ff_cnt << endl;
    cerr << "Output File name :" << fileName << endl;
    cerr << "Cutoff Point :" << ff_cnt + instrument_cnt << endl;

    // Register function to be called to instrument traces
    TRACE_AddInstrumentFunction(Trace, 0);

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
