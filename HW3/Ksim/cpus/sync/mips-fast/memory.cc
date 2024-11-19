#include "pipeline.h"
#include "memory.h"

MemWb::MemWb (ExecMem *em) {
   _pc = em->_pc;
   _ins = em->_ins;

   _decodedSRC3 = em->_decodedSRC3;
   _decodedDST = em->_decodedDST;
   _writeREG = em->_writeREG;
   _writeFREG = em->_writeFREG;
   _hiWPort = em->_hiWPort;
   _loWPort = em->_loWPort;
   _memControl = em->_memControl;
   _opResultHi = em->_opResultHi;
   _opResultLo = em->_opResultLo;
   _MAR = em->_MAR;
   _subregOperand = em->_subregOperand;

   _isSyscall = em->_isSyscall;
   _isIllegalOp = em->_isIllegalOp;

   _memOp = em->_memOp;
}
MemWb::MemWb (void) 
{
   _ins = 0;
   _pc = 0;

   _decodedSRC3 = 0;
   _decodedDST = 0;
   _writeREG = FALSE;
   _writeFREG = FALSE;
   _hiWPort = FALSE;
   _loWPort = FALSE;
   _memControl = FALSE;
   _subregOperand = 0;
   _opResultHi = 0;
   _opResultLo = 0;
   _MAR = 0;

   _isSyscall = 0;
   _isIllegalOp = 0;

   _memOp = NULL;
}
MemWb::~MemWb (void) {}

void
MemWb::copy (MemWb *mw) 
{
   _pc = mw->_pc;
   _ins = mw->_ins;

   _decodedSRC3 = mw->_decodedSRC3;
   _decodedDST = mw->_decodedDST;
   _writeREG = mw->_writeREG;
   _writeFREG = mw->_writeFREG;
   _hiWPort = mw->_hiWPort;
   _loWPort = mw->_loWPort;
   _memControl = mw->_memControl;
   _opResultHi = mw->_opResultHi;
   _opResultLo = mw->_opResultLo;
   _MAR = mw->_MAR;
   _subregOperand = mw->_subregOperand;

   _isSyscall = mw->_isSyscall;
   _isIllegalOp = mw->_isIllegalOp;

   _memOp = mw->_memOp;
}

Memory::Memory (Mipc *mc)
{
   _mc = mc;
}

Memory::~Memory (void) {}

void
Memory::MainLoop (void)
{
   Bool memControl;

   while (1) {
      AWAIT_P_PHI0;	// @posedge
      MemWb *mw = new MemWb(_mc->_em);
      if (_mc->_em->_sreg2 != 0 && _mc->_em->_memControl == TRUE) {
         mw->_subregOperand = _mc->_gprState[_mc->_em->_sreg2];
         if (_mc->_em->_freg != 0)
            mw->_decodedSRC3 = _mc->_fprState[(_mc->_em->_freg)>>1].l[FP_TWIDDLE^((_mc->_em->_freg)&1)];
         else
            mw->_decodedSRC3 = _mc->_gprState[_mc->_em->_sreg2];
     }

      AWAIT_P_PHI1;       // @negedge
      _mc->_mw->copy(mw);
      if (_mc->_mw->_memControl) {
         _mc->_mw->_memOp (_mc, _mc->_mw);

         unsigned decodedDST = _mc->_mw->_decodedDST;
         if (_mc->_mw->_writeREG) { 
            _mc->_gprState[decodedDST] = _mc->_mw->_opResultLo;
#ifdef MIPC_DEBUG
            fprintf(_mc->_debugLog, "<%llu> Memop: Writing to reg %u, value: %#x\n", SIM_TIME, decodedDST, _mc->_mw->_opResultLo);
#endif
         }
         else if (_mc->_mw->_writeFREG) 
            _mc->_fprState[(decodedDST)>>1].l[FP_TWIDDLE^((decodedDST)&1)] = _mc->_mw->_opResultLo;
         else if (_mc->_mw->_loWPort || _mc->_mw->_hiWPort) {
            if (_mc->_mw->_loWPort) 
               _mc->_gprState[LO] = _mc->_mw->_opResultLo;
            if (_mc->_mw->_hiWPort) 
               _mc->_gprState[HI] = _mc->_mw->_opResultHi;
         }
         _mc->_gprState[0] = 0;
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> Accessing memory at address %#x for ins %#x\n", SIM_TIME, _mc->_mw->_MAR, _mc->_mw->_ins);
	      fflush(_mc->_debugLog);
#endif
      }
      else {
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> Memory has nothing to do for ins %#x\n", SIM_TIME, _mc->_mw->_ins);
	      fflush(_mc->_debugLog);
#endif
      }
   }
}
