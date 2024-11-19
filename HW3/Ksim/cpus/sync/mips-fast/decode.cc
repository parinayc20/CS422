#include "pipeline.h"
#include "decode.h"

DecodeExec::DecodeExec(FetchDecode *fd) 
{
   _ins = fd->_ins;
   _pc = fd->_pc;
}
DecodeExec::DecodeExec(void) 
{
   _ins = 0;
   _prevIns = 0;
   _pc = 0;
   _prevPc = 0;
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

   _isSyscall = 0;
   _isIllegalOp = 0;

   _bd = 0;
   _btgt = 0xdeadbeef;

   _sreg1 = 0;
   _sreg2 = 0;
   _freg = 0;

   _opControl = NULL;
   _memOp = NULL;
}
DecodeExec::~DecodeExec (void) {}

void 
DecodeExec::copy(DecodeExec *de) 
{
   _ins = de->_ins;
   _pc = de->_pc;
}

Decode::Decode (Mipc *mc)
{
   _mc = mc;
}

Decode::~Decode (void) {}

void
Decode::MainLoop (void)
{
   unsigned int ins;

   while (1) {
      AWAIT_P_PHI0;	// @posedge
      if (_mc->_interlock)
      {
         _mc->_pc = _mc->_fd->_pc;
         _mc->_nfetched --;
         _mc->_load_stall ++;
         _mc->_fd->_ins = _mc->_de->_prevIns;
         _mc->_fd->_pc = _mc->_de->_prevPc;
      }
      DecodeExec *de = new DecodeExec(_mc->_fd);
      Bool stall = _mc->_stallDec;
      if (_mc->_isSyscall) {
         _mc->_pc = _mc->_fd->_pc;
         _mc->_fd->_ins = 0;    // TODO: Check if this is what was meant by nullifying the instructions
         _mc->_nfetched -= 1;
         _mc->_isSyscall = FALSE;
      }
      for (int i=0; i < 34; i++) {
         if (_mc->_gprCycles[i] != 0)
            _mc->_gprCycles[i] --;
      }
      for (int i=0; i < 16; i++) {
         if (_mc->_fprCycles[i] != 0)
            _mc->_fprCycles[i] --;
      }

      AWAIT_P_PHI1;	// @negedge
      if (!stall) {
         _mc->_de->copy(de);
         _mc->_de->_bd = 0;
         _mc->_de->_sreg1 = _mc->_de->_sreg2 = _mc->_de->_freg = 0;
         _mc->_dcyc = 0;
         _mc->_interlock = FALSE;
         _mc->_de->Dec(_mc, _mc->_em, _mc->_mw, _mc->_de->_ins);

         if(_mc->_de->_isSyscall) {
            _mc->_stallFetch = TRUE;
            _mc->_stallDec = TRUE;
            _mc->_isSyscall = TRUE;
         }
         else if ((_mc->_de->_sreg1 != 0 && _mc->_gprCycles[_mc->_de->_sreg1] > _mc->_scyc1)
                  || (_mc->_de->_sreg2 != 0 && _mc->_gprCycles[_mc->_de->_sreg2] > _mc->_scyc2) 
                  || (_mc->_de->_freg != 0 && _mc->_fprCycles[_mc->_de->_freg] > _mc->_fcyc))
         {
            _mc->_de->_prevIns = _mc->_de->_ins;
            _mc->_de->_prevPc = _mc->_de->_pc;
            _mc->_interlock = TRUE;
            _mc->_de->_ins = 0;
            _mc->_de->_bd = 0;
            _mc->_de->Dec(_mc, _mc->_em, _mc->_mw, _mc->_de->_ins);
         }
         else if (!_mc->_de->_isIllegalOp) {
            if (_mc->_de->_writeREG && _mc->_de->_decodedDST != 0)
               _mc->_gprCycles[_mc->_de->_decodedDST] = _mc->_dcyc;
            else if (_mc->_de->_writeFREG)
               _mc->_fprCycles[(_mc->_de->_decodedDST)>>1] = _mc->_dcyc;
            else if (_mc->_de->_loWPort)
               _mc->_gprCycles[LO] = _mc->_dcyc;
            else if (_mc->_de->_hiWPort)
               _mc->_gprCycles[HI] = _mc->_dcyc;
         }
      }
      else {
         _mc->_de->_ins = 0;
         _mc->_de->_bd = 0;
         _mc->_de->Dec(_mc, _mc->_em, _mc->_mw, _mc->_de->_ins);
      }
#ifdef MIPC_DEBUG
      fprintf(_mc->_debugLog, "<%llu> Decoded ins %#x\n", SIM_TIME, _mc->_de->_ins);
      fflush(_mc->_debugLog);
#endif
   }
}
