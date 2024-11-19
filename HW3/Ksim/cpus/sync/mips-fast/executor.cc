#include "pipeline.h"
#include "executor.h"

ExecMem::ExecMem (DecodeExec *de) 
{
   _ins = de->_ins;
   _pc = de->_pc;

   _decodedSRC1 = de->_decodedSRC1;
   _decodedSRC2 = de->_decodedSRC2;
   _decodedSRC3 = de->_decodedSRC3;
   _decodedDST = de->_decodedDST;		
   _writeREG = de->_writeREG;
   _writeFREG = de->_writeFREG;
   _hiWPort = de->_hiWPort;
   _loWPort = de->_loWPort;
   _memControl = de->_memControl;
   _decodedShiftAmt = de->_decodedShiftAmt;
   _branchOffset = de->_branchOffset;
   _subregOperand = de->_subregOperand;
   
   _isSyscall = de->_isSyscall;
   _isIllegalOp = de->_isIllegalOp;

   _bd = de->_bd;
   _btgt = de->_btgt;

   _sreg2 = de->_sreg2;
   _freg = de->_freg;

   _opControl = de->_opControl;
   _memOp = de->_memOp;
}
ExecMem::ExecMem (void) 
{
   _ins = 0;
   _pc = 0;

   _decodedSRC1 = 0;
   _decodedSRC2 = 0;
   _decodedSRC3 = 0;
   _decodedDST = 0;
   _writeREG = FALSE;
   _writeFREG = FALSE;
   _hiWPort = FALSE;
   _loWPort = FALSE;
   _memControl = FALSE;
   _decodedShiftAmt = 0;
   _branchOffset = 0;
   _subregOperand = 0;
   _opResultHi = 0;
   _opResultLo = 0;
   _MAR = 0;

   _isSyscall = 0;
   _isIllegalOp = 0;

   _bd = 0;
   _btgt = 0xdeadbeef;
   _hi = 0;
   _lo = 0;
   _btaken = 0;

   _opControl = NULL;
   _memOp = NULL;

   _sreg2 = 0;
   _freg = 0;

   _num_jal = 0;
   _num_jr = 0;
   _num_cond_br = 0;
   _num_load = 0;
   _num_store = 0;
}
ExecMem::~ExecMem (void) {}

void
ExecMem::copy (ExecMem *em) 
{
   _ins = em->_ins;
   _pc = em->_pc;

   _decodedSRC1 = em->_decodedSRC1;
   _decodedSRC2 = em->_decodedSRC2;
   _decodedSRC3 = em->_decodedSRC3;
   _decodedDST = em->_decodedDST;		
   _writeREG = em->_writeREG;
   _writeFREG = em->_writeFREG;
   _hiWPort = em->_hiWPort;
   _loWPort = em->_loWPort;
   _memControl = em->_memControl;
   _decodedShiftAmt = em->_decodedShiftAmt;
   _branchOffset = em->_branchOffset;
   _subregOperand = em->_subregOperand;
   
   _isSyscall = em->_isSyscall;
   _isIllegalOp = em->_isIllegalOp;

   _bd = em->_bd;
   _btgt = em->_btgt;

   _opControl = em->_opControl;
   _memOp = em->_memOp;

   _sreg2 = em->_sreg2;
   _freg = em->_freg;

   _hi = em->_hi;
   _lo = em->_lo;
}

Exe::Exe (Mipc *mc)
{
   _mc = mc;
}

Exe::~Exe (void) {}

void
Exe::MainLoop (void)
{
   unsigned int ins;
   Bool isSyscall, isIllegalOp;

   while (1) {
      AWAIT_P_PHI0;	// @posedge
      ExecMem *em = new ExecMem(_mc->_de);

      if (_mc->_de->_sreg1 != 0)
         em->_decodedSRC1 = _mc->_gprState[_mc->_de->_sreg1];
      if (_mc->_de->_sreg2 != 0 && _mc->_de->_memControl == FALSE)
         em->_decodedSRC2 = _mc->_gprState[_mc->_de->_sreg2];
      em->_hi = _mc->_gprState[HI];
      em->_lo = _mc->_gprState[LO];
      if (_mc->_de->_freg != 0)
         em->_decodedSRC1 = _mc->_fprState[(_mc->_de->_freg)>>1].l[FP_TWIDDLE^((_mc->_de->_freg)&1)];

      if (!em->_isIllegalOp && !em->_isSyscall && em->_bd == 1)       // Instruction is a branch instruction
      {
         em->_opControl(em, ins);
         if (em->_btaken) {
            _mc->_pc = em->_btgt;
         }
      }

      AWAIT_P_PHI1;	// @negedge
      _mc->_em->copy(em);
      // if(em->_pc == 0x40555C)
        // _mc->dumpregs();

      ins = _mc->_em->_ins;
      if (!_mc->_em->_isSyscall && !_mc->_em->_isIllegalOp) {
         _mc->_em->_opControl(_mc->_em,ins);

         if (_mc->_em->_memControl == FALSE) 
         {
            unsigned decodedDST = _mc->_em->_decodedDST;
            if (_mc->_em->_writeREG) { 
               _mc->_gprState[decodedDST] = _mc->_em->_opResultLo;
#ifdef MIPC_DEBUG
               fprintf(_mc->_debugLog, "<%llu> Writing to state reg %u, value: %#x\n", SIM_TIME, decodedDST, _mc->_em->_opResultLo);
#endif
            }
            else if (_mc->_em->_writeFREG) 
               _mc->_fprState[(decodedDST)>>1].l[FP_TWIDDLE^((decodedDST)&1)] = _mc->_em->_opResultLo;
            else if (_mc->_em->_loWPort || _mc->_em->_hiWPort) {
               if (_mc->_em->_loWPort) { 
                  _mc->_gprState[LO] = _mc->_em->_opResultLo;
#ifdef MIPC_DEBUG
               fprintf(_mc->_debugLog, "<%llu> Writing to state reg Lo, value: %#x\n", SIM_TIME, _mc->_em->_opResultLo);
#endif
               }
               if (_mc->_em->_hiWPort) {
                  _mc->_gprState[HI] = _mc->_em->_opResultHi;
#ifdef MIPC_DEBUG
               fprintf(_mc->_debugLog, "<%llu> Writing to state reg Hi, value: %#x\n", SIM_TIME, _mc->_em->_opResultHi);
#endif
               }
            }
            _mc->_gprState[0] = 0;
         }
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> Executed ins %#x\n", SIM_TIME, ins);
	      fflush(_mc->_debugLog);
#endif
      }
      else if (isSyscall) {
         //TODO: Zero out pipeline registers
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> Deferring execution of syscall ins %#x\n", SIM_TIME, ins);
	      fflush(_mc->_debugLog);
#endif
      }
      else {
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> Illegal ins %#x in execution stage at PC %#x\n", SIM_TIME, ins, _mc->_pc);
	      fflush(_mc->_debugLog);
#endif
      }
   }
}
