#include "wb.h"

Writeback::Writeback (Mipc *mc)
{
   _mc = mc;
}

Writeback::~Writeback (void) {}

void
Writeback::MainLoop (void)
{
   unsigned int ins;
   unsigned int pc;
   Bool writeReg;
   Bool writeFReg;
   Bool loWPort;
   Bool hiWPort;
   Bool isSyscall;
   Bool isIllegalOp;
   unsigned decodedDST;
   unsigned opResultLo, opResultHi;

   while (1) {
      AWAIT_P_PHI0;	// @posedge
      // Sample the important signals
      writeReg = _mc->_mw->_writeREG;
      writeFReg = _mc->_mw->_writeFREG;
      loWPort = _mc->_mw->_loWPort;
      hiWPort = _mc->_mw->_hiWPort;
      decodedDST = _mc->_mw->_decodedDST;
      opResultLo = _mc->_mw->_opResultLo;
      opResultHi = _mc->_mw->_opResultHi;
      isSyscall = _mc->_mw->_isSyscall;
      isIllegalOp = _mc->_mw->_isIllegalOp;
      ins = _mc->_mw->_ins;
      pc = _mc->_mw->_pc;

      if (!isIllegalOp && !isSyscall) {
         if (writeReg) {
            _mc->_gpr[decodedDST] = opResultLo;
#ifdef MIPC_DEBUG
            fprintf(_mc->_debugLog, "<%llu> Writing to reg %u, value: %#x\n", SIM_TIME, decodedDST, opResultLo);
#endif
         }
         else if (writeFReg) {
            _mc->_fpr[(decodedDST)>>1].l[FP_TWIDDLE^((decodedDST)&1)] = opResultLo;
#ifdef MIPC_DEBUG
            fprintf(_mc->_debugLog, "<%llu> Writing to freg %u, value: %#x\n", SIM_TIME, decodedDST>>1, opResultLo);
#endif
         }
         else if (loWPort || hiWPort) {
            if (loWPort) {
               _mc->_lo = opResultLo;
#ifdef MIPC_DEBUG
               fprintf(_mc->_debugLog, "<%llu> Writing to Lo, value: %#x\n", SIM_TIME, opResultLo);
#endif
            }
            if (hiWPort) {
               _mc->_hi = opResultHi;
#ifdef MIPC_DEBUG
               fprintf(_mc->_debugLog, "<%llu> Writing to Hi, value: %#x\n", SIM_TIME, opResultHi);
#endif
            }
         }
#ifdef MIPC_DEBUG
      fflush(_mc->_debugLog);
#endif
      }
      _mc->_gpr[0] = 0;

      AWAIT_P_PHI1;       // @negedge
      if (isSyscall) {
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> SYSCALL! Trapping to emulation layer at PC %#x\n", SIM_TIME, pc);
    	 fflush(_mc->_debugLog);
#endif      
         _mc->fake_syscall (pc);
         _mc->_stallFetch = FALSE;
         _mc->_stallDec = FALSE;
         for (int i = 0; i < 34; i++)
            _mc->_gprState[i] = _mc->_gpr[i];
      }
      else if (isIllegalOp) {
         printf("Illegal ins %#x at PC %#x. Terminating simulation!\n", ins, pc);
#ifdef MIPC_DEBUG
         fclose(_mc->_debugLog);
#endif
         printf("Register state on termination:\n\n");
         _mc->dumpregs();
         exit(0);
      }
   }
}
