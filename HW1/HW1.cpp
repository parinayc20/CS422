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
#include <iomanip>
#include <math.h>
#include <unordered_set>
#include <vector>
#include <chrono>

using namespace std;

#define ONE_BILLION 1e+9

std::chrono::high_resolution_clock::time_point t_start;
std::chrono::high_resolution_clock::time_point t_end;
/* ================================================================== */
// Structures
/* ================================================================== */

typedef struct InsCategoryCount {
    // Will be instrumented for only true predicates
    UINT32 loads;
    UINT32 stores;
    UINT32 nop;
    UINT32 directCall;
    UINT32 indirectCall;
    UINT32 returns;
    UINT32 unconditionalBranch;
    UINT32 conditionalBranch;
    UINT32 logicalOps;
    UINT32 rotateAndShift;
    UINT32 flagOps;
    UINT32 vectorIns;
    UINT32 conditionalMoves;
    UINT32 mmxSSEIns;
    UINT32 syscalls;
    UINT32 fpIns;
    UINT32 others;
} InsCategoryCount;

/* ================================================================== */
// Global variables
/* ================================================================== */

InsCategoryCount insCategoryCount = {0}; // dynamic count of different category ins executed
UINT32 insCategoryTotalCount = 0;
UINT32 CPI;

UINT64 fastForwardCount;
UINT64 insCount    = 0; //number of dynamically executed instructions
UINT64 analysedInsCount = 0;

unordered_set<UINT32> unqiue_data;
unordered_set<UINT32> unique_ins;

vector<UINT32> insLenCount(16, 0);
vector<UINT32> numOperandsCount(8, 0);
vector<UINT32> regReadOperandsCount(8,0);
vector<UINT32> regWriteOperandsCount(8,0);
vector<UINT32> memOpsCount(8,0);
vector<UINT32> readMemOpsCount(8,0);
vector<UINT32> writeMemOpsCount(8,0);
UINT32 maxBytesAccessed = 0;
UINT64 avgBytesAccessed = 0;
UINT32 totalMemIns = 0; // instruction having atleast one memory operands

INT32 maxImmediate = -INT_MAX;
INT32 minImmediate = INT_MAX;

ADDRDELTA minDisp = INT_MAX;
ADDRDELTA maxDisp = -INT_MAX;

std::ostream* out = &cerr;
string outputFile;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name for MyPinTool output");

KNOB< INT64 > KnobFastForwardCount(KNOB_MODE_WRITEONCE, "pintool", "f", "", "specify fast forward amount in multiples of 1 billion");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool prints out the dynamic counts/percentages of " << endl
         << "instructions executed in different spec2006 benchmark programs." << endl
         << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

template<typename T> 
inline void printElement(T t)
{
    *out << left << setw(30) << t;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

/*!
 * Increase counter of the executed basic blocks and instructions.
 * This function is called for every basic block when it is about to be executed.
 * @param[in]   numInstInBbl    number of instructions in the basic block
 * @note use atomic operations for multi-threaded applications
 */

// Predicated
VOID IncCategoryCounter(UINT32* count){
    (*count) ++;
}

// Predicated
VOID MemoryAnalysis(UINT32* catAddr, UINT32 numLoadsStores, UINT32 rwSize, VOID* addr, ADDRINT disp){
    *(catAddr) += numLoadsStores;
    
    // JAYA
    // data footprint
    UINT32 addri = (UINT32)(addr);
    
    UINT32 start = (addri>>5);
    UINT32 end = ((addri+rwSize) >> 5);

    for(UINT32 i = start; i<=end; i++) {
        unqiue_data.insert(i);
    }

    ADDRDELTA displacement = (ADDRDELTA)(disp);
    if(displacement > maxDisp)
        maxDisp = displacement;

    if(displacement < minDisp) 
        minDisp = displacement;
}

// Predicated
VOID MemoryOperandCount(UINT32 loadOps, UINT32 storeOps, UINT32 totalBytesAccessed) {
    readMemOpsCount[loadOps] += 1;
    writeMemOpsCount[storeOps] += 1;
    memOpsCount[loadOps+storeOps] += 1;

    if(loadOps+storeOps){
        avgBytesAccessed += totalBytesAccessed;
        if(totalBytesAccessed > maxBytesAccessed){
            maxBytesAccessed = totalBytesAccessed;
        }

        totalMemIns += 1;  // instruction having atleast one memory operands
    }
}

// Non Predicated
void InstructionAnalysis(VOID* ip, UINT32 insSize, UINT32 numOperands, UINT32 regReadCount, UINT32 regWriteCount) {

    analysedInsCount ++;
    
    // JAYA
    // instruction footprint
    UINT32 ipa = (UINT32)(ip);
    
    UINT32 start = (ipa>>5);
    UINT32 end = ((ipa+insSize) >> 5);

    for(UINT32 i = start; i<=end; i++) {
        unique_ins.insert(i);
    }

    insLenCount[insSize] += 1;
    numOperandsCount[numOperands] += 1;
    regReadOperandsCount[regReadCount] += 1;
    regWriteOperandsCount[regWriteCount] += 1;
}

// Non Predicated
VOID ImmediateCount(INT32 immediate) {
    if(immediate > maxImmediate) {
        maxImmediate = immediate;
    }

    if(immediate < minImmediate) {
        minImmediate = immediate;
    }
}

// VOID CountIns(UINT32 numInstInBbl)
// {
//     insCount += numInstInBbl;
// }

// Non Predicated
VOID CountIns()
{
    insCount ++;
}

// Terminate condition
ADDRINT Terminate(void)
{
    return (insCount >= fastForwardCount + ONE_BILLION);
}

// Analysis routine to check fast-forward condition
ADDRINT FastForward (void) {
	return ((insCount >= fastForwardCount) && insCount);
}

// Analysis routine to exit the application
void MyExitRoutine (void) {
	// Do an exit system call to exit the application.
	// As we are calling the exit system call PIN would not be able to instrument application end.
	// Because of this, even if you are instrumenting the application end, the Fini function would not
	// be called. Thus you should report the statistics here, before doing the exit system call.

    // Print the stats here and then exit
    printElement("Total Ins Count: "); printElement(insCount); *out << "\n";
    printElement("Analysed Ins Count: "); printElement(analysedInsCount); *out<< "\n";
    *out << endl;
    *out << endl;

    // Calculate total ins executed
    insCategoryTotalCount = (insCategoryCount.loads) +
                            (insCategoryCount.stores) +
                            (insCategoryCount.nop) +
                            (insCategoryCount.directCall) +
                            (insCategoryCount.indirectCall) +
                            (insCategoryCount.returns) +
                            (insCategoryCount.unconditionalBranch) +
                            (insCategoryCount.conditionalBranch) +
                            (insCategoryCount.logicalOps) +
                            (insCategoryCount.rotateAndShift) +
                            (insCategoryCount.flagOps) +
                            (insCategoryCount.vectorIns) +
                            (insCategoryCount.conditionalMoves) +
                            (insCategoryCount.mmxSSEIns) +
                            (insCategoryCount.syscalls) +
                            (insCategoryCount.fpIns) +
                            (insCategoryCount.others);

    // Print category stats in a file
    printElement("Category"); printElement("InsCount"); printElement("Percentage"); *out << endl;
    printElement("Loads"); printElement(insCategoryCount.loads); printElement((insCategoryCount.loads/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("Stores"); printElement(insCategoryCount.stores); printElement((insCategoryCount.stores/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("Nop"); printElement(insCategoryCount.nop); printElement((insCategoryCount.nop/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("DirectCall"); printElement(insCategoryCount.directCall); printElement((insCategoryCount.directCall/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("IndirectCall"); printElement(insCategoryCount.indirectCall); printElement((insCategoryCount.indirectCall/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("Returns"); printElement(insCategoryCount.returns); printElement((insCategoryCount.returns/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("UnconditionalBranch"); printElement(insCategoryCount.unconditionalBranch); printElement((insCategoryCount.unconditionalBranch/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("ConditionalBranch"); printElement(insCategoryCount.conditionalBranch); printElement((insCategoryCount.conditionalBranch/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("LogicalOps"); printElement(insCategoryCount.logicalOps); printElement((insCategoryCount.logicalOps/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("RotateAndShift"); printElement(insCategoryCount.rotateAndShift); printElement((insCategoryCount.rotateAndShift/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("FlagOps"); printElement(insCategoryCount.flagOps); printElement((insCategoryCount.flagOps/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("VectorIns"); printElement(insCategoryCount.vectorIns); printElement((insCategoryCount.vectorIns/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("ConditionalMoves"); printElement(insCategoryCount.conditionalMoves); printElement((insCategoryCount.conditionalMoves/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("MmxSSEIns"); printElement(insCategoryCount.mmxSSEIns); printElement((insCategoryCount.mmxSSEIns/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("Syscalls"); printElement(insCategoryCount.syscalls); printElement((insCategoryCount.syscalls/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("FpIns"); printElement(insCategoryCount.fpIns); printElement((insCategoryCount.fpIns/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("Others"); printElement(insCategoryCount.others); printElement((insCategoryCount.others/(float)insCategoryTotalCount) * 100.0); *out << endl;
    *out << endl;
    printElement("Total Ins"); printElement(insCategoryTotalCount); *out << endl;
    
    *out << endl;
    CPI = ceil(((insCategoryCount.loads + insCategoryCount.stores) * 69.0 + (insCategoryTotalCount))/ insCategoryTotalCount);
    printElement("CPI: "); printElement(CPI); *out << endl;

    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    printElement("Instruction Footprint: "); printElement(unique_ins.size()); *out << endl;
    printElement("Data Footprint: "); printElement(unqiue_data.size()); *out << endl;

    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    
    printElement("Distribution of instruction length"); *out<<endl;
    printElement("|"); printElement("Instruction Length"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<insLenCount.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(insLenCount[i]); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Distribution of the number of operands in an instruction"); *out<<endl;
    printElement("|"); printElement("Num Operands"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<numOperandsCount.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(numOperandsCount[i]); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Distribution of the number of register read operands in an instruction"); *out<<endl;
    printElement("|"); printElement("Num REG Read Ops"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<regReadOperandsCount.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(regReadOperandsCount[i]); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Distribution of the number of register write operands in an instruction"); *out<<endl;
    printElement("|"); printElement("Num REG Write Ops"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<regWriteOperandsCount.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(regWriteOperandsCount[i]); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Distribution of the number of memory operands in an instruction"); *out<<endl;
    printElement("|"); printElement("Num Memory Ops"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<memOpsCount.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(memOpsCount[i]); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    
    printElement("Distribution of the number of memory read operands in an instruction"); *out<<endl;
    printElement("|"); printElement("Num Memory Read Ops"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<readMemOpsCount.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(readMemOpsCount[i]); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Distribution of the number of memory write operands in an instruction"); *out<<endl;
    printElement("|"); printElement("Num Memory Write Ops"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<writeMemOpsCount.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(writeMemOpsCount[i]); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Memory Bytes Touched"); *out << endl;
    printElement("Maximum: "); printElement(maxBytesAccessed); *out << endl;
    printElement("Average: "); printElement(avgBytesAccessed/((float)totalMemIns)); *out << endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    
    printElement("Immediate Field value"); *out << endl;
    printElement("Maximum: "); printElement(maxImmediate); *out << endl;
    printElement("Minimum: "); printElement(minImmediate); *out << endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Displacement Field Value"); *out << endl;
    printElement("Maximum: "); printElement(maxDisp); *out << endl;
    printElement("Minimum: "); printElement(minDisp); *out << endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    
    t_end = std::chrono::high_resolution_clock::now();

    auto elapsed_time_s = std::chrono::duration_cast<std::chrono::seconds>(t_end-t_start).count();
    *out << endl;
    *out << endl;
    printElement("Total Execution Time: "); printElement(elapsed_time_s); *out<< endl;
    
    exit(0);
}


/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

inline void CategoryCount(BBL bbl) {
   
    for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
    {
        // FastForward() is called for every ins executed
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);

        // Find instruction footprint
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)InstructionAnalysis, 
                                                IARG_INST_PTR, 
                                                IARG_UINT32, INS_Size(ins), 
                                                IARG_UINT32, INS_OperandCount(ins),
                                                IARG_UINT32, INS_MaxNumRRegs(ins),
                                                IARG_UINT32, INS_MaxNumWRegs(ins),
                                                IARG_END);
        
        for(UINT32 op =0; op < INS_OperandCount(ins); op++) {
            if(INS_OperandIsImmediate(ins, op)){
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                ADDRINT val = INS_OperandImmediate(ins, op);
                INS_InsertThenCall(
                    ins,
                    IPOINT_BEFORE,
                    (AFUNPTR)ImmediateCount,
                    IARG_ADDRINT, val,
                    IARG_END
                );
            }
        }
        
        // Count load and store instructions for type B
        UINT32 memOperands = INS_MemoryOperandCount(ins);
        UINT32 loadOperands = 0, storeOperands = 0;

        UINT32 totalBytesAccessed = 0;
        UINT32 loadCount =0, storeCount = 0;

        for (UINT32 memOp = 0; memOp < memOperands; memOp++)
        {   
            UINT32 rwSize = INS_MemoryOperandSize(ins, memOp);
            totalBytesAccessed += rwSize;

            if (INS_MemoryOperandIsRead(ins, memOp))
            {   
                loadOperands++;
                loadCount += (rwSize >> 2);
                loadCount += ((rwSize & 0x3) ? 1 : 0);

                // FastForward() is called for every ins executed
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(
                    ins,
                    IPOINT_BEFORE,
                    (AFUNPTR)MemoryAnalysis,
                    IARG_PTR, &(insCategoryCount.loads),
                    IARG_UINT32, loadCount,
                    IARG_UINT32, rwSize,
                    IARG_MEMORYOP_EA, memOp,
                    IARG_ADDRINT, INS_OperandMemoryDisplacement(ins, memOp),
                    IARG_END
                );
            }
            // Note that in some architectures a single memory operand can be 
            // both read and written (for instance incl (%eax) on IA-32)
            // In that case we instrument it once for read and once for write.
            if (INS_MemoryOperandIsWritten(ins, memOp))
            {   
                storeOperands++;
                storeCount += (rwSize >> 2);
                storeCount += ((rwSize & 0x3) ? 1 : 0);

                // FastForward() is called for every ins executed
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(
                    ins,
                    IPOINT_BEFORE,
                    (AFUNPTR)MemoryAnalysis,
                    IARG_PTR, &(insCategoryCount.stores),
                    IARG_UINT32, storeCount,
                    IARG_UINT32, rwSize,
                    IARG_MEMORYOP_EA, memOp,
                    IARG_ADDRINT, INS_OperandMemoryDisplacement(ins, memOp),
                    IARG_END
                );
            }
        }

        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenPredicatedCall(
            ins,
            IPOINT_BEFORE,
            (AFUNPTR)MemoryOperandCount,
            IARG_UINT32, loadOperands,
            IARG_UINT32, storeOperands,
            IARG_UINT32, totalBytesAccessed,
            IARG_END
        );

        // Categorize all instructions for type A
        UINT32 category = INS_Category(ins);
        UINT32* aType = NULL;
        
        switch(category) {
            case XED_CATEGORY_NOP:
            {   aType = &(insCategoryCount.nop);
                break;
            }
            case XED_CATEGORY_CALL: 
            {
                if(INS_IsDirectCall(ins)) {
                    // Increment direct call count by one   
                    aType = &(insCategoryCount.directCall);                
                }
                else {  
                    aType = &(insCategoryCount.indirectCall);               
                }

                break;
            }
            case XED_CATEGORY_RET: 
            {   aType = &(insCategoryCount.returns);                    
                break;
            }
            case XED_CATEGORY_UNCOND_BR: 
            {   aType = &(insCategoryCount.unconditionalBranch);                    
                break;
            }
            case XED_CATEGORY_COND_BR: 
            {   aType = &(insCategoryCount.conditionalBranch);                    
                break;
            }
            case XED_CATEGORY_LOGICAL: 
            {   aType = &(insCategoryCount.logicalOps);                    
                break;
            }
            case XED_CATEGORY_ROTATE:
            case XED_CATEGORY_SHIFT:
            {   aType = &(insCategoryCount.rotateAndShift);
                break;
            }
            case XED_CATEGORY_FLAGOP: 
            {   aType = &(insCategoryCount.flagOps);                    
                break;
            }
            case XED_CATEGORY_AVX:
            case XED_CATEGORY_AVX2:
            case XED_CATEGORY_AVX2GATHER:
            case XED_CATEGORY_AVX512:
            {   aType = &(insCategoryCount.vectorIns);
                break;
            }
            case XED_CATEGORY_CMOV: 
            {   aType = &(insCategoryCount.conditionalMoves);                    
                break;
            }
            case XED_CATEGORY_MMX:
            case XED_CATEGORY_SSE:
            {   aType = &(insCategoryCount.mmxSSEIns);
                break;
            }
            case XED_CATEGORY_SYSCALL: 
            {   aType = &(insCategoryCount.syscalls);                    
                break;
            }
            case XED_CATEGORY_X87_ALU: 
            {   aType = &(insCategoryCount.fpIns);                    
                break;
            }
            default:
            {   aType = &(insCategoryCount.others);
                break;
            }
        }

        // FastForward() is called for every ins executed
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenPredicatedCall(
            ins,
            IPOINT_BEFORE,
            (AFUNPTR)IncCategoryCounter,
            IARG_PTR, aType,
            IARG_END
        );

        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountIns, IARG_END);
    }
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

        // MyExitRoutine() is called only when the last call returns a non-zero value.
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)MyExitRoutine, IARG_END);

        // Insert all the calls here onc     e you have fast forwarded the given amount of ins
        CategoryCount(bbl);

        // Insert a call to CountBbl() before every basic block, passing the number of instructions
        // BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)CountIns, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
    }
}


/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddFiniFunction function call
 * 
 * This function will never be called in our case. We will use exit condition to terminate the program.
 */
VOID Fini(INT32 code, VOID* v)
{
    MyExitRoutine();
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

    outputFile = KnobOutputFile.Value();

    cout << "outFile: " << outputFile << endl;
    if (!outputFile.empty())
    {
        out = new std::ofstream(outputFile.c_str());
    }

    fastForwardCount = KnobFastForwardCount.Value();
    fastForwardCount *= ONE_BILLION;
    cout << "FF count: " << fastForwardCount << endl;

    // Register function to be called to instrument traces
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    cerr << "===============================================" << endl;
    cerr << "This application is instrumented by MyPinTool" << endl;
    if (!KnobOutputFile.Value().empty())
    {
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    }
    cerr << "===============================================" << endl;

    t_start = std::chrono::high_resolution_clock::now();
    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
