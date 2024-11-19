#include "pipeline.h"
#include "mips.h"
#include <assert.h>
#include "mips-irix5.h"

FetchDecode::FetchDecode (void) 
{
   _ins = 0;
   _pc = 0;
}
FetchDecode::~FetchDecode (void) {}

Mipc::Mipc (Mem *m) : _l('M')
{
   _mem = m;
   _sys = new MipcSysCall (this);	// Allocate syscall layer

#ifdef MIPC_DEBUG
   _debugLog = fopen("mipc.debug", "w");
   assert(_debugLog != NULL);
#endif
   
   Reboot (ParamGetString ("Mipc.BootROM"));
}

Mipc::~Mipc (void)
{

}

void 
Mipc::MainLoop (void)
{
   LL addr;
   unsigned int ins;	// Local instruction register

   Assert (_boot, "Mipc::MainLoop() called without boot?");

   _nfetched = 0;

   while (!_sim_exit) {
      AWAIT_P_PHI0;	// @posedge
      Bool stall = _stallFetch;

      AWAIT_P_PHI1;	// @negedge
      if (!stall) {
         addr = _pc;
         ins = _mem->BEGetWord (addr, _mem->Read(addr & ~(LL)0x7));
#ifdef MIPC_DEBUG
         fprintf(_debugLog, "<%llu> Fetched ins %#x from PC %#x\n", SIM_TIME, ins, _pc);
         fflush(_debugLog);
#endif
         _fd->_ins = ins;
         _fd->_pc = addr;
         _pc = _pc + 4;
         _nfetched++;
      }
      // _bd = 0;
   }

   MipcDumpstats();
   Log::CloseLog();
   
#ifdef MIPC_DEBUG
   assert(_debugLog != NULL);
   fclose(_debugLog);
#endif

   exit(0);
}

void
Mipc::MipcDumpstats()
{
  Log l('*');
  l.startLogging = 0;

  l.print ("");
  l.print ("************************************************************");
  l.print ("");
  l.print ("Number of instructions: %llu", _nfetched);
  l.print ("Number of simulated cycles: %llu", SIM_TIME);
  l.print ("CPI: %.2f", ((double)SIM_TIME)/_nfetched);
  l.print ("Int Conditional Branches: %llu", _em->_num_cond_br);
  l.print ("Jump and Link: %llu", _em->_num_jal);
  l.print ("Jump Register: %llu", _em->_num_jr);
  l.print ("Number of fp instructions: %llu", _fpinst);
  l.print ("Number of loads: %llu", _em->_num_load);
  l.print ("Number of syscall emulated loads: %llu", _sys->_num_load);
  l.print ("Number of stores: %llu", _em->_num_store);
  l.print ("Number of syscall emulated stores: %llu", _sys->_num_store);
  l.print ("Number of syscalls: %llu", _num_sys);
  l.print ("Number of load stalls: %llu", _load_stall);
  l.print ("");

}

void 
Mipc::fake_syscall (unsigned int pc)
{
   _sys->pc = pc;
   _sys->quit = 0;
   _sys->EmulateSysCall ();
   _num_sys ++;
   if (_sys->quit)
      _sim_exit = 1;
}

/*------------------------------------------------------------------------
 *
 *  Mipc::Reboot --
 *
 *   Reset processor state
 *
 *------------------------------------------------------------------------
 */
void 
Mipc::Reboot (char *image)
{
   FILE *fp;
   Log l('*');

   _boot = 0;

   if (image) {
      _boot = 1;
      printf ("Executing %s\n", image);
      fp = fopen (image, "r");
      if (!fp) {
	 fatal_error ("Could not open `%s' for booting host!", image);
      }
      _mem->ReadImage(fp);
      fclose (fp);

      // Reset state
      _ins = 0;

      _num_load = 0;
      _num_store = 0;
      _fpinst = 0;
      _num_cond_br = 0;
      _num_jal = 0;
      _num_jr = 0;
      _num_sys = 0;
      _load_stall = 0;

      _fd = new FetchDecode();
      _de = new DecodeExec();
      _em = new ExecMem();
      _mw = new MemWb();

      _stallFetch = FALSE;
      _stallDec = FALSE;

      _de->_opControl = _em->func_sll;

      _sim_exit = 0;
      _pc = ParamGetInt ("Mipc.BootPC");	// Boom! GO
   }
}

LL
MipcSysCall::GetDWord(LL addr)
{
   _num_load++;      
   return m->Read (addr);
}

void
MipcSysCall::SetDWord(LL addr, LL data)
{
  
   m->Write (addr, data);
   _num_store++;
}

Word 
MipcSysCall::GetWord (LL addr) 
{ 
  
   _num_load++;   
   return m->BEGetWord (addr, m->Read (addr & ~(LL)0x7)); 
}

void 
MipcSysCall::SetWord (LL addr, Word data) 
{ 
  
   m->Write (addr & ~(LL)0x7, m->BESetWord (addr, m->Read(addr & ~(LL)0x7), data)); 
   _num_store++;
}
  
void 
MipcSysCall::SetReg (int reg, LL val) 
{ 
   _ms->_gpr[reg] = val; 
}

LL 
MipcSysCall::GetReg (int reg) 
{
   return _ms->_gpr[reg]; 
}

LL
MipcSysCall::GetTime (void)
{
  return SIM_TIME;
}
