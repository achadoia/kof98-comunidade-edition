// Contexto do 68000 pro savestate: o contexto do Musashi contém ponteiros (tabelas de ciclos e callbacks)
// que mudam entre execuções/builds; ao carregar, eles são repostos a partir do contexto vivo.
#include "m68kcpu.h"
#include <string.h>

unsigned int m68k_estado_tam(void) { return sizeof(m68ki_cpu_core); }

void m68k_estado_pega(void *dst) { memcpy(dst, &m68ki_cpu, sizeof m68ki_cpu); }

void m68k_estado_poe(const void *src)
{
	m68ki_cpu_core vivo = m68ki_cpu;
	memcpy(&m68ki_cpu, src, sizeof m68ki_cpu);
	m68ki_cpu.cyc_instruction = vivo.cyc_instruction;
	m68ki_cpu.cyc_exception = vivo.cyc_exception;
	m68ki_cpu.int_ack_callback = vivo.int_ack_callback;
	m68ki_cpu.bkpt_ack_callback = vivo.bkpt_ack_callback;
	m68ki_cpu.reset_instr_callback = vivo.reset_instr_callback;
	m68ki_cpu.cmpild_instr_callback = vivo.cmpild_instr_callback;
	m68ki_cpu.rte_instr_callback = vivo.rte_instr_callback;
	m68ki_cpu.tas_instr_callback = vivo.tas_instr_callback;
	m68ki_cpu.illg_instr_callback = vivo.illg_instr_callback;
	m68ki_cpu.trap_instr_callback = vivo.trap_instr_callback;
	m68ki_cpu.pc_changed_callback = vivo.pc_changed_callback;
	m68ki_cpu.set_fc_callback = vivo.set_fc_callback;
	m68ki_cpu.instr_hook_callback = vivo.instr_hook_callback;
}
