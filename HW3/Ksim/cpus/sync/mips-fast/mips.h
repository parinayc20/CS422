#ifndef __MIPS_H__
#define __MIPS_H__

#include "sim.h"
#include "pipeline.h"

class Mipc;
class MipcSysCall;
class SysCall;

class FetchDecode;
class DecodeExec;
class ExecMem;
class MemWb;

typedef unsigned Bool;
#define TRUE 1
#define FALSE 0

#define LO 32
#define HI 33

#if BYTE_ORDER == LITTLE_ENDIAN

#define FP_TWIDDLE 0

#else

#define FP_TWIDDLE 1

#endif

#include "mem.h"
#include "../../common/syscall.h"
#include "queue.h"

// #define MIPC_DEBUG 1

class Mipc : public SimObject {
public:
   Mipc (Mem *m);
   ~Mipc ();
  
   FAKE_SIM_TEMPLATE;

   MipcSysCall *_sys;		// Emulated system call layer

   void dumpregs (void);	// Dumps current register state

   void Reboot (char *image = NULL);
				// Restart processor.
				// "image" = file name for new memory
				// image if any.

   void MipcDumpstats();			// Prints simulation statistics
   void fake_syscall (unsigned int pc);	// System call interface

   /* processor state */
   FetchDecode *_fd;
   DecodeExec *_de;
   ExecMem *_em;
   MemWb *_mw;

   unsigned int _ins;         // instruction register
   Bool     _stallFetch;
   Bool     _stallDec;

   unsigned int 	_gpr[32];		// general-purpose integer registers
   unsigned int   _gprState[34];
   unsigned int   _gprCycles[34];  

   union {
      unsigned int l[2];
      float f[2];
      double d;
   } _fpr[16];					// floating-point registers (paired)
   union {
      unsigned int l[2];
      float f[2];
      double d;
   } _fprState[16];
   unsigned int   _fprCycles[16];

   unsigned int _hi, _lo; 			// mult, div destination
   unsigned int	_pc;				// Program counter
   unsigned int _boot;				// boot code loaded?

   Bool		_isSyscall;			// 1 if system call
   Bool     _interlock;

   // Simulation statistics counters

   LL	_nfetched;
   LL	_num_cond_br;
   LL	_num_jal;
   LL	_num_jr;
   LL   _num_load;
   LL   _num_store;
   LL   _fpinst;
   LL   _num_sys;
   LL   _load_stall;

   Mem	*_mem;	// attached memory (not a cache)

   Log	_l;
   int  _sim_exit;		// 1 on normal termination

   unsigned int _scyc1, _scyc2;
   unsigned int _fcyc;
   unsigned int _dcyc;

   FILE *_debugLog;
};


// Emulated system call interface

class MipcSysCall : public SysCall {
public:

   MipcSysCall (Mipc *ms) {

      char buf[1024];
      m = ms->_mem;
      _ms = ms;
      _num_load = 0;
      _num_store = 0;
   };

   ~MipcSysCall () { };

   LL GetDWord (LL addr);
   void SetDWord (LL addr, LL data);

   Word GetWord (LL addr);
   void SetWord (LL addr, Word data);
  
   void SetReg (int reg, LL val);
   LL GetReg (int reg);
   LL GetTime (void);

private:

   Mipc *_ms;
};
#endif /* __MIPS_H__ */
