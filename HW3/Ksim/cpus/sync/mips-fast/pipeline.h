#ifndef __PIPELINE_H__
#define __PIPELINE_H__

#include "mem.h"
#include "mips.h"

class Mipc;
class FetchDecode;
class DecodeExec;
class ExecMem;
class MemWb;

typedef unsigned Bool;
typedef unsigned long long LL;

class FetchDecode {
public:
    unsigned int _ins;
    unsigned int _pc;

    FetchDecode ();
    ~FetchDecode ();
};

class DecodeExec {
public:
    unsigned int _ins, _prevIns;
    unsigned int _pc, _prevPc;

    signed int	_decodedSRC1, _decodedSRC2, _decodedSRC3;	// Reg fetch output (source values)
    unsigned	_decodedDST;			// Decoder output (dest reg no)
    Bool		_writeREG, _writeFREG;		// WB control
    Bool 	_hiWPort, _loWPort;		// WB control
    Bool 	_memControl;			// Memory instruction?
    unsigned	_decodedShiftAmt;		// Shift amount
    signed int	_branchOffset;
    unsigned 	_subregOperand;			// Needed for lwl and lwr
    
    Bool		_isSyscall;			// 1 if system call
    Bool		_isIllegalOp;			// 1 if illegal opcode

    int 		_bd;				// 1 if the next ins is delay slot
    unsigned int	_btgt;				// branch target

    unsigned int _sreg1, _sreg2;
    unsigned int _freg;

    void (*_opControl)(ExecMem*, unsigned);
    void (*_memOp)(Mipc*, MemWb*);

    DecodeExec (FetchDecode *fd);
    DecodeExec ();
    ~DecodeExec ();

    void Dec (Mipc* mc, ExecMem* em, MemWb* mw, unsigned int ins);			// Decoder function
    void copy (DecodeExec *de);
};

class ExecMem {
public:
    unsigned int _ins;
    unsigned int _pc;

    signed int	_decodedSRC1, _decodedSRC2, _decodedSRC3;	// Reg fetch output (source values)
    unsigned	_decodedDST;			// Decoder output (dest reg no)
    Bool		_writeREG, _writeFREG;		// WB control
    Bool 	_hiWPort, _loWPort;		// WB control
    Bool 	_memControl;			// Memory instruction?
    unsigned	_decodedShiftAmt;		// Shift amount
    signed int	_branchOffset;
    unsigned	_opResultHi, _opResultLo;	// Result of operation
    unsigned	_MAR;				// Memory address register
    unsigned 	_subregOperand;			// Needed for lwl and lwr


    Bool		_isSyscall;			// 1 if system call
    Bool		_isIllegalOp;			// 1 if illegal opcode

    unsigned int _hi, _lo; 			// mult, div destination
    int 		_bd;				// 1 if the next ins is delay slot
    unsigned int	_btgt;				// branch target
    int 		_btaken; 			// taken branch (1 if taken, 0 if fall-through)

    void (*_opControl)(ExecMem*, unsigned);
    void (*_memOp)(Mipc*, MemWb*);

    LL	_num_jal;
    LL	_num_jr;
    LL	_num_cond_br;
    LL   _num_load;
    LL   _num_store;

    unsigned int _sreg2;
    unsigned int _freg;

    ExecMem (DecodeExec *de);
    ExecMem ();
    ~ExecMem ();

    void copy (ExecMem *em);

    // EXE stage definitions

    static void func_add_addu (ExecMem*, unsigned);
    static void func_and (ExecMem*, unsigned);
    static void func_nor (ExecMem*, unsigned);
    static void func_or (ExecMem*, unsigned);
    static void func_sll (ExecMem*, unsigned);
    static void func_sllv (ExecMem*, unsigned);
    static void func_slt (ExecMem*, unsigned);
    static void func_sltu (ExecMem*, unsigned);
    static void func_sra (ExecMem*, unsigned);
    static void func_srav (ExecMem*, unsigned);
    static void func_srl (ExecMem*, unsigned);
    static void func_srlv (ExecMem*, unsigned);
    static void func_sub_subu (ExecMem*, unsigned);
    static void func_xor (ExecMem*, unsigned);
    static void func_div (ExecMem*, unsigned);
    static void func_divu (ExecMem*, unsigned);
    static void func_mfhi (ExecMem*, unsigned);
    static void func_mflo (ExecMem*, unsigned);
    static void func_mthi (ExecMem*, unsigned);
    static void func_mtlo (ExecMem*, unsigned);
    static void func_mult (ExecMem*, unsigned);
    static void func_multu (ExecMem*, unsigned);
    static void func_jalr (ExecMem*, unsigned);
    static void func_jr (ExecMem*, unsigned);
    static void func_await_break (ExecMem*, unsigned);
    static void func_syscall (ExecMem*, unsigned);
    static void func_addi_addiu (ExecMem*, unsigned);
    static void func_andi (ExecMem*, unsigned);
    static void func_lui (ExecMem*, unsigned);
    static void func_ori (ExecMem*, unsigned);
    static void func_slti (ExecMem*, unsigned);
    static void func_sltiu (ExecMem*, unsigned);
    static void func_xori (ExecMem*, unsigned);
    static void func_beq (ExecMem*, unsigned);
    static void func_bgez (ExecMem*, unsigned);
    static void func_bgezal (ExecMem*, unsigned);
    static void func_bltzal (ExecMem*, unsigned);
    static void func_bltz (ExecMem*, unsigned);
    static void func_bgtz (ExecMem*, unsigned);
    static void func_blez (ExecMem*, unsigned);
    static void func_bne (ExecMem*, unsigned);
    static void func_j (ExecMem*, unsigned);
    static void func_jal (ExecMem*, unsigned);
    static void func_lb (ExecMem*, unsigned);
    static void func_lbu (ExecMem*, unsigned);
    static void func_lh (ExecMem*, unsigned);
    static void func_lhu (ExecMem*, unsigned);
    static void func_lwl (ExecMem*, unsigned);
    static void func_lw (ExecMem*, unsigned);
    static void func_lwr (ExecMem*, unsigned);
    static void func_lwc1 (ExecMem*, unsigned);
    static void func_swc1 (ExecMem*, unsigned);
    static void func_sb (ExecMem*, unsigned);
    static void func_sh (ExecMem*, unsigned);
    static void func_swl (ExecMem*, unsigned);
    static void func_sw (ExecMem*, unsigned);
    static void func_swr (ExecMem*, unsigned);
    static void func_mtc1 (ExecMem*, unsigned);
    static void func_mfc1 (ExecMem*, unsigned);
};

class MemWb {
public:
    unsigned int _pc;
    unsigned int _ins;

    signed int  _decodedSRC3;
    unsigned	_decodedDST;			// Decoder output (dest reg no)
    Bool		_writeREG, _writeFREG;		// WB control
    Bool 	_hiWPort, _loWPort;		// WB control
    Bool 	_memControl;			// Memory instruction?
    unsigned	_opResultHi, _opResultLo;	// Result of operation
    unsigned	_MAR;				// Memory address register
    unsigned 	_subregOperand;			// Needed for lwl and lwr

    Bool		_isSyscall;			// 1 if system call
    Bool		_isIllegalOp;			// 1 if illegal opcode

    void (*_memOp)(Mipc*, MemWb*);

    MemWb (ExecMem* em);
    MemWb ();
    ~MemWb ();

    void copy (MemWb *mw);

    // MEM stage definitions

   static void mem_lb (Mipc* mc, MemWb*);
   static void mem_lbu (Mipc* mc, MemWb*);
   static void mem_lh (Mipc* mc, MemWb*);
   static void mem_lhu (Mipc* mc, MemWb*);
   static void mem_lwl (Mipc* mc, MemWb*);
   static void mem_lw (Mipc* mc, MemWb*);
   static void mem_lwr (Mipc* mc, MemWb*);
   static void mem_lwc1 (Mipc* mc, MemWb*);
   static void mem_swc1 (Mipc* mc, MemWb*);
   static void mem_sb (Mipc* mc, MemWb*);
   static void mem_sh (Mipc* mc, MemWb*);
   static void mem_swl (Mipc* mc, MemWb*);
   static void mem_sw (Mipc* mc, MemWb*);
   static void mem_swr (Mipc* mc, MemWb*);
};

#endif
