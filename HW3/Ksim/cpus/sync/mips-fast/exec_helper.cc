#include <math.h>
#include "pipeline.h"
#include "mips.h"
#include "opcodes.h"
#include <assert.h>
#include "app_syscall.h"

/*------------------------------------------------------------------------
 *
 *  Instruction exec 
 *
 *------------------------------------------------------------------------
 */
void
DecodeExec::Dec (Mipc *_mc, ExecMem *_em, MemWb *_mw, unsigned int ins)
{
   MipsInsn i;
   signed int a1, a2;
   unsigned int ar1, ar2, s1, s2, r1, r2, t1, t2;
   LL addr;
   unsigned int val;
   LL value, mask;
   int sa,j;
   Word dummy;

   _isIllegalOp = FALSE;
   _isSyscall = FALSE;

   i.data = ins;
  
#define SIGN_EXTEND_BYTE(x)  do { x <<= 24; x >>= 24; } while (0)
#define SIGN_EXTEND_IMM(x)   do { x <<= 16; x >>= 16; } while (0)

   switch (i.reg.op) {
   case 0:
      // SPECIAL (ALU format)
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = _mc->_gpr[i.reg.rt];
      _decodedDST = i.reg.rd;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _sreg1 = i.reg.rs;
      _sreg2 = i.reg.rt;
      _mc->_scyc1 = _mc->_scyc2 = 1;
      _mc->_dcyc = 2;

      switch (i.reg.func) {
      case 0x20:			// add
      case 0x21:			// addu
         _opControl = _em->func_add_addu;
	 break;

      case 0x24:			// and
         _opControl = _em->func_and;
	 break;

      case 0x27:			// nor
         _opControl = _em->func_nor;
	 break;

      case 0x25:			// or
         _opControl = _em->func_or;
	 break;

      case 0:			// sll
         _opControl = _em->func_sll;
         _decodedShiftAmt = i.reg.sa;
	 break;

      case 4:			// sllv
         _opControl = _em->func_sllv;
	 break;

      case 0x2a:			// slt
         _opControl = _em->func_slt;
	 break;

      case 0x2b:			// sltu
         _opControl = _em->func_sltu;
	 break;

      case 0x3:			// sra
         _opControl = _em->func_sra;
         _decodedShiftAmt = i.reg.sa;
	 break;

      case 0x7:			// srav
         _opControl = _em->func_srav;
	 break;

      case 0x2:			// srl
         _opControl = _em->func_srl;
         _decodedShiftAmt = i.reg.sa;
	 break;

      case 0x6:			// srlv
         _opControl = _em->func_srlv;
	 break;

      case 0x22:			// sub
      case 0x23:			// subu
	 // no overflow check
         _opControl = _em->func_sub_subu;
	 break;

      case 0x26:			// xor
         _opControl = _em->func_xor;
	 break;
      _sreg1 = i.reg.rs;
      case 0x1a:			// div
         _opControl = _em->func_div;
         _hiWPort = TRUE;
         _loWPort = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;

      case 0x1b:			// divu
         _opControl = _em->func_divu;
         _hiWPort = TRUE;
         _loWPort = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;

      case 0x10:			// mfhi
         _opControl = _em->func_mfhi;
         _sreg1 = HI;
	 break;

      case 0x12:			// mflo
         _opControl = _em->func_mflo;
         _sreg1 = LO;
	 break;

      case 0x11:			// mthi
         _opControl = _em->func_mthi;
         _hiWPort = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;

      case 0x13:			// mtlo
         _opControl = _em->func_mtlo;
         _loWPort = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;

      case 0x18:			// mult
         _opControl = _em->func_mult;
         _hiWPort = TRUE;
         _loWPort = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;

      case 0x19:			// multu
         _opControl = _em->func_multu;
         _hiWPort = TRUE;
         _loWPort = TRUE;
         _writeREG = FALSE;
          _writeFREG = FALSE;
	 break;

      case 9:			// jalr
         _opControl = _em->func_jalr;
         _btgt = _decodedSRC1;
         _bd = 1;
         break;

      case 8:			// jr
         _opControl = _em->func_jr;
         _writeREG = FALSE;
         _writeFREG = FALSE;
         _btgt = _decodedSRC1;
         _bd = 1;
	 break;

      case 0xd:			// await/break
         _opControl = _em->func_await_break;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;

      case 0xc:			// syscall
         _opControl = _em->func_syscall;
         _writeREG = FALSE;
         _writeFREG = FALSE;
         _isSyscall = TRUE;
	 break;

      default:
	      _isIllegalOp = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;
      }
      break;	// ALU format

   case 8:			// addi
   case 9:			// addiu
      // ignore overflow: no exceptions
      _opControl = _em->func_addi_addiu;
      _decodedSRC1 = _mc->_gpr[i.imm.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
       _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _sreg1 = i.imm.rs;
      _mc->_scyc1 = 1;
      _mc->_dcyc = 2;
      break;

   case 0xc:			// andi
      _opControl = _em->func_andi;
      _decodedSRC1 = _mc->_gpr[i.imm.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _sreg1 = i.imm.rs;
      _mc->_scyc1 = 1;
      _mc->_dcyc = 2;
      break;

   case 0xf:			// lui
      _opControl = _em->func_lui;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _mc->_dcyc = 2;
      break;

   case 0xd:			// ori
      _opControl = _em->func_ori;
      _decodedSRC1 = _mc->_gpr[i.imm.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _sreg1 = i.imm.rs;
      _mc->_scyc1 = 1;
      _mc->_dcyc = 2;
      break;

   case 0xa:			// slti
      _opControl = _em->func_slti;
      _decodedSRC1 = _mc->_gpr[i.imm.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _sreg1 = i.imm.rs;
      _mc->_scyc1 = 1;
      _mc->_dcyc = 2;
      break;

   case 0xb:			// sltiu
      _opControl = _em->func_sltiu;
      _decodedSRC1 = _mc->_gpr[i.imm.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _sreg1 = i.imm.rs;
      _mc->_scyc1 = 1;
      _mc->_dcyc = 2;
      break;

   case 0xe:			// xori
      _opControl = _em->func_xori;
      _decodedSRC1 = _mc->_gpr[i.imm.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _sreg1 = i.imm.rs;
      _mc->_scyc1 = 1;
      _mc->_dcyc = 2;
      break;

   case 4:			// beq
      _opControl = _em->func_beq;
      _decodedSRC1 = _mc->_gpr[i.imm.rs];
      _decodedSRC2 = _mc->_gpr[i.imm.rt];
      _branchOffset = i.imm.imm;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
      _sreg1 = i.imm.rs;
      _sreg2 = i.imm.rt;
      _mc->_scyc1 = _mc->_scyc2 = 1;
      _mc->_dcyc = 2;
      break;

   case 1:
      // REGIMM
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _branchOffset = i.imm.imm;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _sreg1 = i.reg.rs;
      _mc->_scyc1 = 1;

      switch (i.reg.rt) {
      case 1:			// bgez
         _opControl = _em->func_bgez;
         _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
	 break;

      case 0x11:			// bgezal
         _opControl = _em->func_bgezal;
         _decodedDST = 31;
         _writeREG = TRUE;
         _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
         _mc->_dcyc = 2;
	 break;

      case 0x10:			// bltzal
         _opControl = _em->func_bltzal;
         _decodedDST = 31;
         _writeREG = TRUE;
         _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
         _mc->_dcyc = 2;
	 break;

      case 0x0:			// bltz
         _opControl = _em->func_bltz;
         _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
	 break;

      default:
	 _isIllegalOp = TRUE;
	 break;
      }
      break;

   case 7:			// bgtz
      _opControl = _em->func_bgtz;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _branchOffset = i.imm.imm;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
      _sreg1 = i.reg.rs;
      _mc->_scyc1 = 1;
      break;

   case 6:			// blez
      _opControl = _em->func_blez;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _branchOffset = i.imm.imm;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
      _sreg1 = i.reg.rs;
      _mc->_scyc1 = 1;
      break;

   case 5:			// bne
      _opControl = _em->func_bne;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = _mc->_gpr[i.reg.rt];
      _branchOffset = i.imm.imm;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
      _sreg1 = i.reg.rs;
      _sreg2 = i.reg.rt;
      _mc->_scyc1 = _mc->_scyc2 = 1;
      break;

   case 2:			// j
      _opControl = _em->func_j;
      _branchOffset = i.tgt.tgt;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _btgt = ((_pc+4) & 0xf0000000) | (_branchOffset<<2); _bd = 1;
      break;

   case 3:			// jal
      _opControl = _em->func_jal;
      _branchOffset = i.tgt.tgt;
      _decodedDST = 31;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _btgt = ((_pc+4) & 0xf0000000) | (_branchOffset<<2); _bd = 1;
      _mc->_dcyc = 2;
      break;

   case 0x20:			// lb  
      _opControl = _em->func_lb;
      _memOp = _mw->mem_lb;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _mc->_scyc1 = 1;
      _mc->_dcyc = 3;
      break;

   case 0x24:			// lbu
      _opControl = _em->func_lbu;
      _memOp = _mw->mem_lbu;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _mc->_scyc1 = 1;
      _mc->_dcyc = 3;
      break;

   case 0x21:			// lh
      _opControl = _em->func_lh;
      _memOp = _mw->mem_lh;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _mc->_scyc1 = 1;
      _mc->_dcyc = 3;
      break;

   case 0x25:			// lhu
      _opControl = _em->func_lhu;
      _memOp = _mw->mem_lhu;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _mc->_scyc1 = 1;
      _mc->_dcyc = 3;
      break;

   case 0x22:			// lwl
      _opControl = _em->func_lwl;
      _memOp = _mw->mem_lwl;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _subregOperand = _mc->_gpr[i.reg.rt];
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _sreg2 = i.reg.rt;
      _mc->_scyc1 = 1;
      _mc->_scyc2 = 2;
      _mc->_dcyc = 3;
      break;

   case 0x23:			// lw
      _opControl = _em->func_lw;
      _memOp = _mw->mem_lw;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _mc->_scyc1 = 1;
      _mc->_dcyc = 3;
      break;

   case 0x26:			// lwr
      _opControl = _em->func_lwr;
      _memOp = _mw->mem_lwr;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _subregOperand = _mc->_gpr[i.reg.rt];
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _sreg2 = i.reg.rt;
      _mc->_scyc1 = 1;
      _mc->_scyc2 = 2;
      _mc->_dcyc = 3;
      break;

   case 0x31:			// lwc1
      _opControl = _em->func_lwc1;
      _memOp = _mw->mem_lwc1;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = FALSE;
      _writeFREG = TRUE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _mc->_scyc1 = 1;
      _mc->_dcyc = 3;
      break;

   case 0x39:			// swc1
      _opControl = _em->func_swc1;
      _memOp = _mw->mem_swc1;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedSRC3 = _mc->_fpr[(i.reg.rt)>>1].l[FP_TWIDDLE^((i.reg.rt)&1)];
      _decodedDST = 0;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _freg = i.reg.rt;
      _mc->_scyc1 = 1;
      _mc->_fcyc = 2;
      break;

   case 0x28:			// sb
      _opControl = _em->func_sb;
      _memOp = _mw->mem_sb;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedSRC3 = _mc->_gpr[i.reg.rt];
      _decodedDST = 0;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _sreg2 = i.reg.rt;
      _mc->_scyc1 = 1;
      _mc->_scyc2 = 2;
      break;

   case 0x29:			// sh  store half word
      _opControl = _em->func_sh;
      _memOp = _mw->mem_sh;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedSRC3 = _mc->_gpr[i.reg.rt];
      _decodedDST = 0;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _sreg2 = i.reg.rt;
      _mc->_scyc1 = 1;
      _mc->_scyc2 = 2;
      break;

   case 0x2a:			// swl
      _opControl = _em->func_swl;
      _memOp = _mw->mem_swl;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedSRC3 = _mc->_gpr[i.reg.rt];
      _decodedDST = 0;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _sreg2 = i.reg.rt;
      _mc->_scyc1 = 1;
      _mc->_scyc2 = 2;
      break;

   case 0x2b:			// sw
      _opControl = _em->func_sw;
      _memOp = _mw->mem_sw;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedSRC3 = _mc->_gpr[i.reg.rt];
      _decodedDST = 0;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _sreg2 = i.reg.rt;
      _mc->_scyc1 = 1;
      _mc->_scyc2 = 2;
      break;

   case 0x2e:			// swr
      _opControl = _em->func_swr;
      _memOp = _mw->mem_swr;
      _decodedSRC1 = _mc->_gpr[i.reg.rs];
      _decodedSRC2 = i.imm.imm;
      _decodedSRC3 = _mc->_gpr[i.reg.rt];
      _decodedDST = 0;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      _sreg1 = i.reg.rs;
      _sreg2 = i.reg.rt;
      _mc->_scyc1 = 1;
      _mc->_scyc2 = 2;
      break;

   case 0x11:			// floating-point
      _mc->_fpinst++;
      switch (i.freg.fmt) {
      case 4:			// mtc1
         _opControl = _em->func_mtc1;
         _decodedSRC1 = _mc->_gpr[i.freg.ft];
         _decodedDST = i.freg.fs;
         _writeREG = FALSE;
         _writeFREG = TRUE;
         _hiWPort = FALSE;
         _loWPort = FALSE;
         _memControl = FALSE;
         _sreg1 = i.freg.ft;   //TODO: Change this in previous implementation as well
         _mc->_scyc1 = 1;
         _mc->_dcyc = 2;
	 break;

      case 0:			// mfc1
         _opControl = _em->func_mfc1;
         _decodedSRC1 = _mc->_fpr[(i.freg.fs)>>1].l[FP_TWIDDLE^((i.freg.fs)&1)];
         _decodedDST = i.freg.ft;
         _writeREG = TRUE;
         _writeFREG = FALSE;
         _hiWPort = FALSE;
         _loWPort = FALSE;
         _memControl = FALSE;
         _freg = i.freg.ft;
         _mc->_fcyc = 1;
         _mc->_dcyc = 2;
	 break;
      default:
         _isIllegalOp = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
         _hiWPort = FALSE;
         _loWPort = FALSE;
         _memControl = FALSE;
	 break;
      }
      break;
   default:
      _isIllegalOp = TRUE;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      break;
   }
}


/*
 *
 * Debugging: print registers
 *
 */
void 
Mipc::dumpregs (void)
{
   int i;

   printf ("\n--- PC = %08x ---\n", _pc);
   for (i=0; i < 32; i++) {
      if (i < 10)
	 printf (" r%d: %08x (%ld)\n", i, _gpr[i], _gpr[i]);
      else
	 printf ("r%d: %08x (%ld)\n", i, _gpr[i], _gpr[i]);
   }
   printf ("taken: %d, bd: %d\n", _em->_btaken, _em->_bd);
   printf ("target: %08x\n", _em->_btgt);
}

void
ExecMem::func_add_addu (ExecMem *em, unsigned ins)
{
   em->_opResultLo = (unsigned)(em->_decodedSRC1 + em->_decodedSRC2);
   // printf("Encountered unimplemented instruction: add or addu.\n");
   // printf("You need to fill in func_add_addu in exec_helper.cc to proceed forward.\n");
   // exit(0);
}

void
ExecMem::func_and (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_decodedSRC1 & em->_decodedSRC2;
}

void
ExecMem::func_nor (ExecMem *em, unsigned ins)
{
   em->_opResultLo = ~(em->_decodedSRC1 | em->_decodedSRC2);
}

void
ExecMem::func_or (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_decodedSRC1 | em->_decodedSRC2;
}

void
ExecMem::func_sll (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_decodedSRC2 << em->_decodedShiftAmt;
}

void
ExecMem::func_sllv (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_decodedSRC2 << (em->_decodedSRC1 & 0x1f);  
   // printf("Encountered unimplemented instruction: sllv.\n");
   // printf("You need to fill in func_sllv in exec_helper.cc to proceed forward.\n");
   // exit(0);
}

void
ExecMem::func_slt (ExecMem *em, unsigned ins)
{
   if (em->_decodedSRC1 < em->_decodedSRC2) {
      em->_opResultLo = 1;
   }
   else {
      em->_opResultLo = 0;
   }
}

void
ExecMem::func_sltu (ExecMem *em, unsigned ins)
{
   if ((unsigned)em->_decodedSRC1 < (unsigned)em->_decodedSRC2) {
      em->_opResultLo = 1;
   }
   else {
      em->_opResultLo = 0;
   }
}

void
ExecMem::func_sra (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_decodedSRC2 >> em->_decodedShiftAmt;
}

void
ExecMem::func_srav (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_decodedSRC2 >> (em->_decodedSRC1 & 0x1f);
}

void
ExecMem::func_srl (ExecMem *em, unsigned ins)
{
   em->_opResultLo = (unsigned)em->_decodedSRC2 >> em->_decodedShiftAmt;
}

void
ExecMem::func_srlv (ExecMem *em, unsigned ins)
{
   em->_opResultLo = (unsigned)em->_decodedSRC2 >> (em->_decodedSRC1 & 0x1f);
}

void
ExecMem::func_sub_subu (ExecMem *em, unsigned ins)
{
   em->_opResultLo = (unsigned)em->_decodedSRC1 - (unsigned)em->_decodedSRC2;
}

void
ExecMem::func_xor (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_decodedSRC1 ^ em->_decodedSRC2;
}

void
ExecMem::func_div (ExecMem *em, unsigned ins)
{
   if (em->_decodedSRC2 != 0) {
      em->_opResultHi = (unsigned)(em->_decodedSRC1 % em->_decodedSRC2);
      em->_opResultLo = (unsigned)(em->_decodedSRC1 / em->_decodedSRC2);
   }
   else {
      em->_opResultHi = 0x7fffffff;
      em->_opResultLo = 0x7fffffff;
   }
}

void
ExecMem::func_divu (ExecMem *em, unsigned ins)
{
   if ((unsigned)em->_decodedSRC2 != 0) {
      em->_opResultHi = (unsigned)(em->_decodedSRC1) % (unsigned)(em->_decodedSRC2);
      em->_opResultLo = (unsigned)(em->_decodedSRC1) / (unsigned)(em->_decodedSRC2);
   }
   else {
      em->_opResultHi = 0x7fffffff;
      em->_opResultLo = 0x7fffffff;
   }
}

void
ExecMem::func_mfhi (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_hi;
}

void
ExecMem::func_mflo (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_lo;
}

void
ExecMem::func_mthi (ExecMem *em, unsigned ins)
{
   em->_opResultHi = em->_decodedSRC1;
}

void
ExecMem::func_mtlo (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_decodedSRC1;
}

void
ExecMem::func_mult (ExecMem *em, unsigned ins)
{
   unsigned int ar1, ar2, s1, s2, r1, r2, t1, t2;
                                                                                
   ar1 = em->_decodedSRC1;
   ar2 = em->_decodedSRC2;
   s1 = ar1 >> 31; if (s1) ar1 = 0x7fffffff & (~ar1 + 1);
   s2 = ar2 >> 31; if (s2) ar2 = 0x7fffffff & (~ar2 + 1);
                                                                                
   t1 = (ar1 & 0xffff) * (ar2 & 0xffff);
   r1 = t1 & 0xffff;              // bottom 16 bits
                                                                                
   // compute next set of 16 bits
   t1 = (ar1 & 0xffff) * (ar2 >> 16) + (t1 >> 16);
   t2 = (ar2 & 0xffff) * (ar1 >> 16);
                                                                                
   r1 = r1 | (((t1+t2) & 0xffff) << 16); // bottom 32 bits
   r2 = (ar1 >> 16) * (ar2 >> 16) + (t1 >> 16) + (t2 >> 16) +
            (((t1 & 0xffff) + (t2 & 0xffff)) >> 16);
                                                                                
   if (s1 ^ s2) {
      r1 = ~r1;
      r2 = ~r2;
      r1++;
      if (r1 == 0)
         r2++;
   }
   em->_opResultHi = r2;
   em->_opResultLo = r1;
}

void
ExecMem::func_multu (ExecMem *em, unsigned ins)
{
   unsigned int ar1, ar2, s1, s2, r1, r2, t1, t2;
                                                                                
   ar1 = em->_decodedSRC1;
   ar2 = em->_decodedSRC2;
                                                                                
   t1 = (ar1 & 0xffff) * (ar2 & 0xffff);
   r1 = t1 & 0xffff;              // bottom 16 bits
                                                                                
   // compute next set of 16 bits
   t1 = (ar1 & 0xffff) * (ar2 >> 16) + (t1 >> 16);
   t2 = (ar2 & 0xffff) * (ar1 >> 16);
                                                                                
   r1 = r1 | (((t1+t2) & 0xffff) << 16); // bottom 32 bits
   r2 = (ar1 >> 16) * (ar2 >> 16) + (t1 >> 16) + (t2 >> 16) +
            (((t1 & 0xffff) + (t2 & 0xffff)) >> 16);
                            
   em->_opResultHi = r2;
   em->_opResultLo = r1;                                                    
}

void
ExecMem::func_jalr (ExecMem *em, unsigned ins)
{
   em->_btgt = em->_decodedSRC1;
   em->_btaken = 1;
   em->_num_jal++;
   em->_opResultLo = em->_pc + 8;
}

void
ExecMem::func_jr (ExecMem *em, unsigned ins)
{
   em->_btgt = em->_decodedSRC1;
   em->_btaken = 1;
   em->_num_jr++;
}

void
ExecMem::func_await_break (ExecMem *em, unsigned ins)
{
}

void
ExecMem::func_syscall (ExecMem *em, unsigned ins)
{
   // have to change this function to raise a flag
   // already have the flag _isSyscall
   // em->fake_syscall (ins);
}

void
ExecMem::func_addi_addiu (ExecMem *em, unsigned ins)
{
   // printf("Encountered unimplemented instruction: addi or addiu.\n");
   // printf("You need to fill in func_addi_addiu in exec_helper.cc to proceed forward.\n");
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_opResultLo = (unsigned)(em->_decodedSRC1 + em->_decodedSRC2);
   // exit(0);
}

void
ExecMem::func_andi (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_decodedSRC1 & em->_decodedSRC2;
}

void
ExecMem::func_lui (ExecMem *em, unsigned ins)
{
   // printf("Encountered unimplemented instruction: lui.\n");
   // printf("You need to fill in func_lui in exec_helper.cc to proceed forward.\n");
   em->_opResultLo = (em->_decodedSRC2 << 16);
   // exit(0);
}

void
ExecMem::func_ori (ExecMem *em, unsigned ins)
{
   // printf("Encountered unimplemented instruction: ori.\n");
   // printf("You need to fill in func_ori in exec_helper.cc to proceed forward.\n");
   em->_opResultLo = em->_decodedSRC1 | em->_decodedSRC2;
   // exit(0);
}

void
ExecMem::func_slti (ExecMem *em, unsigned ins)
{
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   if (em->_decodedSRC1 < em->_decodedSRC2) {
      em->_opResultLo = 1;
   }
   else {
      em->_opResultLo = 0;
   }
}

void
ExecMem::func_sltiu (ExecMem *em, unsigned ins)
{
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   if ((unsigned)em->_decodedSRC1 < (unsigned)em->_decodedSRC2) {
      em->_opResultLo = 1;
   }
   else {
      em->_opResultLo = 0;
   }
}

void
ExecMem::func_xori (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_decodedSRC1 ^ em->_decodedSRC2;
}

void
ExecMem::func_beq (ExecMem *em, unsigned ins)
{
   em->_num_cond_br++;
   em->_btaken = (em->_decodedSRC1 == em->_decodedSRC2);
   // printf("Encountered unimplemented instruction: beq.\n");
   // printf("You need to fill in func_beq in exec_helper.cc to proceed forward.\n");
   // exit(0);
}

void
ExecMem::func_bgez (ExecMem *em, unsigned ins)
{
   em->_num_cond_br++;
   em->_btaken = !(em->_decodedSRC1 >> 31);
}

void
ExecMem::func_bgezal (ExecMem *em, unsigned ins)
{
   em->_num_cond_br++;
   em->_btaken = !(em->_decodedSRC1 >> 31);
   em->_opResultLo = em->_pc + 8;
}

void
ExecMem::func_bltzal (ExecMem *em, unsigned ins)
{
   em->_num_cond_br++;
   em->_btaken = (em->_decodedSRC1 >> 31);
   em->_opResultLo = em->_pc + 8;
}

void
ExecMem::func_bltz (ExecMem *em, unsigned ins)
{
   em->_num_cond_br++;
   em->_btaken = (em->_decodedSRC1 >> 31);
}

void
ExecMem::func_bgtz (ExecMem *em, unsigned ins)
{
   em->_num_cond_br++;
   em->_btaken = (em->_decodedSRC1 > 0);
}

void
ExecMem::func_blez (ExecMem *em, unsigned ins)
{
   em->_num_cond_br++;
   em->_btaken = (em->_decodedSRC1 <= 0);
}

void
ExecMem::func_bne (ExecMem *em, unsigned ins)
{
   em->_num_cond_br++;
   em->_btaken = (em->_decodedSRC1 != em->_decodedSRC2);
}

void
ExecMem::func_j (ExecMem *em, unsigned ins)
{
   em->_btaken = 1;
}

void
ExecMem::func_jal (ExecMem *em, unsigned ins)
{
   em->_num_jal++;
   em->_btaken = 1;
   em->_opResultLo = em->_pc + 8;
   // printf("Encountered unimplemented instruction: jal.\n");
   // printf("You need to fill in func_jal in exec_helper.cc to proceed forward.\n");
   // exit(0);
}

void
ExecMem::func_lb (ExecMem *em, unsigned ins)
{
   signed int a1;

   em->_num_load++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1+em->_decodedSRC2);
}

void
ExecMem::func_lbu (ExecMem *em, unsigned ins)
{
   em->_num_load++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1+em->_decodedSRC2);
}

void
ExecMem::func_lh (ExecMem *em, unsigned ins)
{
   signed int a1;
                                                                                
   em->_num_load++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1+em->_decodedSRC2);
}

void
ExecMem::func_lhu (ExecMem *em, unsigned ins)
{
   em->_num_load++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1+em->_decodedSRC2);
}

void
ExecMem::func_lwl (ExecMem *em, unsigned ins)
{
   signed int a1;
   unsigned s1;
                                                                                
   em->_num_load++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1+em->_decodedSRC2);
}

void
ExecMem::func_lw (ExecMem *em, unsigned ins)
{
   signed int a1;

   em->_num_load++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1 + em->_decodedSRC2);
   // printf("Encountered unimplemented instruction: lw.\n");
   // printf("You need to fill in func_lw in exec_helper.cc to proceed forward.\n");
   // exit(0);
}

void
ExecMem::func_lwr (ExecMem *em, unsigned ins)
{
   unsigned ar1, s1;
                                                                                
   em->_num_load++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1+em->_decodedSRC2);
}

void
ExecMem::func_lwc1 (ExecMem *em, unsigned ins)
{
   em->_num_load++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1+em->_decodedSRC2);
}

void
ExecMem::func_swc1 (ExecMem *em, unsigned ins)
{
   em->_num_store++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1+em->_decodedSRC2);
}

void
ExecMem::func_sb (ExecMem *em, unsigned ins)
{
   em->_num_store++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1+em->_decodedSRC2);
}

void
ExecMem::func_sh (ExecMem *em, unsigned ins)
{
   em->_num_store++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1+em->_decodedSRC2);
}

void
ExecMem::func_swl (ExecMem *em, unsigned ins)
{
   unsigned ar1, s1;
                                                                                
   em->_num_store++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1+em->_decodedSRC2);
}

void
ExecMem::func_sw (ExecMem *em, unsigned ins)
{
   em->_num_store++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1+em->_decodedSRC2);
}

void
ExecMem::func_swr (ExecMem *em, unsigned ins)
{
   unsigned ar1, s1;
                                                                                
   em->_num_store++;
   SIGN_EXTEND_IMM(em->_decodedSRC2);
   em->_MAR = (unsigned)(em->_decodedSRC1+em->_decodedSRC2);
}

void
ExecMem::func_mtc1 (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_decodedSRC1;
}

void
ExecMem::func_mfc1 (ExecMem *em, unsigned ins)
{
   em->_opResultLo = em->_decodedSRC1;
}



void
MemWb::mem_lb (Mipc *mc, MemWb *mw)
{
   signed int a1;

   a1 = mc->_mem->BEGetByte(mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7));
   SIGN_EXTEND_BYTE(a1);
   mw->_opResultLo = a1;
}

void
MemWb::mem_lbu (Mipc *mc, MemWb *mw)
{
   mw->_opResultLo = mc->_mem->BEGetByte(mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7));
}

void
MemWb::mem_lh (Mipc *mc, MemWb *mw)
{
   signed int a1;

   a1 = mc->_mem->BEGetHalfWord(mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7));
   SIGN_EXTEND_IMM(a1);
   mw->_opResultLo = a1;
}

void
MemWb::mem_lhu (Mipc *mc, MemWb *mw)
{
   mw->_opResultLo = mc->_mem->BEGetHalfWord (mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7));
}

void
MemWb::mem_lwl (Mipc *mc, MemWb *mw)
{
   signed int a1;
   unsigned s1;

   a1 = mc->_mem->BEGetWord (mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7));
   s1 = (mw->_MAR & 3) << 3;
   mw->_opResultLo = (a1 << s1) | (mw->_subregOperand & ~(~0UL << s1));
}

void
MemWb::mem_lw (Mipc *mc, MemWb *mw)
{
   mw->_opResultLo = mc->_mem->BEGetWord (mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7));
}

void
MemWb::mem_lwr (Mipc *mc, MemWb *mw)
{
   unsigned ar1, s1;

   ar1 = mc->_mem->BEGetWord (mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7));
   s1 = (~mw->_MAR & 3) << 3;
   mw->_opResultLo = (ar1 >> s1) | (mw->_subregOperand & ~(~(unsigned)0 >> s1));
#ifdef MIPC_DEBUG
   fprintf(mc->_debugLog, "<%llu> mem_lwr: MAR: %#x, ar1: %#x, s1: %#x, res: %#x, subregOp: %#x\n", SIM_TIME, mw->_MAR, ar1, s1, mw->_opResultLo, mw->_subregOperand);
#endif
}

void
MemWb::mem_lwc1 (Mipc *mc, MemWb *mw)
{
   mw->_opResultLo = mc->_mem->BEGetWord (mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7));
}

void
MemWb::mem_swc1 (Mipc *mc, MemWb *mw)
{
   mc->_mem->Write(mw->_MAR & ~(LL)0x7, mc->_mem->BESetWord (mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7), mw->_decodedSRC3));
}

void
MemWb::mem_sb (Mipc *mc, MemWb *mw)
{
   mc->_mem->Write(mw->_MAR & ~(LL)0x7, mc->_mem->BESetByte (mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7), mw->_decodedSRC3 & 0xff));
}

void
MemWb::mem_sh (Mipc *mc, MemWb *mw)
{
   mc->_mem->Write(mw->_MAR & ~(LL)0x7, mc->_mem->BESetHalfWord (mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7), mw->_decodedSRC3 & 0xffff));
}

void
MemWb::mem_swl (Mipc *mc, MemWb *mw)
{
   unsigned ar1, s1;

   ar1 = mc->_mem->BEGetWord (mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7));
   s1 = (mw->_MAR & 3) << 3;
   ar1 = (mw->_decodedSRC3 >> s1) | (ar1 & ~(~(unsigned)0 >> s1));
   mc->_mem->Write(mw->_MAR & ~(LL)0x7, mc->_mem->BESetWord (mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7), ar1));
}

void
MemWb::mem_sw (Mipc *mc, MemWb *mw)
{
   mc->_mem->Write(mw->_MAR & ~(LL)0x7, mc->_mem->BESetWord (mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7), mw->_decodedSRC3));
}

void
MemWb::mem_swr (Mipc *mc, MemWb *mw)
{
   unsigned ar1, s1;

   ar1 = mc->_mem->BEGetWord (mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7));
   s1 = (~mw->_MAR & 3) << 3;
   ar1 = (mw->_decodedSRC3 << s1) | (ar1 & ~(~0UL << s1));
   mc->_mem->Write(mw->_MAR & ~(LL)0x7, mc->_mem->BESetWord (mw->_MAR, mc->_mem->Read(mw->_MAR & ~(LL)0x7), ar1));
}
