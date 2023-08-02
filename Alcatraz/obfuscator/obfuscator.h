#pragma once
#include "../pe/pe.h"
#include "Zydis/Zydis.h"
#include "../pdbparser/pdbparser.h"
#include "../../Alcatraz-gui/gui/gui.h"
#include <string>
#include <algorithm>
#include <unordered_map>
#include <asmjit/asmjit.h>
#include <random>
using namespace asmjit;
static int currentFuncId = LONG_MAX - 0x100000;
static int currentInstrId = LONG_MAX - 0x100000;
#define NEXT_FUNC() (currentFuncId++)
#define NEXT_INSTR() (currentInstrId++)

void asmJitIsBad(std::vector<uint8_t> bytes, x86::Assembler* trash);

bool is_jmpcall(ZydisDecodedInstruction instr);
bool is_stackop(ZydisDecodedInstruction instr);
bool is_ripop(ZydisDecodedInstruction instr);
ULONG64 getSyscallId();
ULONG64 getRand64();

class obfuscator {
private:
	struct instruction_t;
	struct function_t;
	pe64* pe;

	static int instruction_id;
	static int function_iterator;

	static std::unordered_map<ZydisRegister_, x86::Gp>lookupmap;

	JitRuntime rt;
	CodeHolder code;
	x86::Assembler assm;

	std::vector<function_t>functions;

	uint32_t total_size_used;

	bool find_inst_at_dst(uint64_t dst, instruction_t** instptr, function_t** funcptr);

	void remove_jumptables();

	bool analyze_functions();

	void relocate(PIMAGE_SECTION_HEADER new_section);

	bool find_instruction_by_id(int funcid, int instid, instruction_t* inst);

	bool fix_relative_jmps(function_t* func);

	bool convert_relative_jmps();

	bool apply_relocations(PIMAGE_SECTION_HEADER new_section);

	void compile(PIMAGE_SECTION_HEADER new_section);

	std::vector<instruction_t>instructions_from_jit(uint8_t* code, uint32_t size);

	/*
		Miscellaneous
	*/

	bool flatten_control_flow(std::vector<obfuscator::function_t>::iterator& func_iter);
	bool obfuscate_iat_call(std::vector<obfuscator::function_t>::iterator& func_iter, std::vector<obfuscator::instruction_t>::iterator& instruction_iter);
	void add_custom_entry(PIMAGE_SECTION_HEADER* new_section);

	/*
		These are our actual obfuscation passes
	*/

	bool obfuscate_lea(std::vector<obfuscator::function_t>::iterator& func_iter, std::vector<obfuscator::instruction_t>::iterator& instruction_iter);
	bool obfuscate_ff(std::vector<obfuscator::function_t>::iterator& func_iter, std::vector<obfuscator::instruction_t>::iterator& instruction_iter);
	bool add_junk(std::vector<obfuscator::function_t>::iterator& func_iter, std::vector<obfuscator::instruction_t>::iterator& instruction_iter);
	bool obfuscate_mov(std::vector<obfuscator::function_t>::iterator& func_iter, std::vector<obfuscator::instruction_t>::iterator& instruction_iter);
	bool obfuscate_add(std::vector<obfuscator::function_t>::iterator& func_iter, std::vector<obfuscator::instruction_t>::iterator& instruction_iter);
	bool obfuscate_pop(std::vector<obfuscator::function_t>::iterator& func_iter, std::vector<obfuscator::instruction_t>::iterator& instruction_iter);
	bool obfuscate_return(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction);
	bool obfuscate_xor(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction);
	bool obfuscate_cmp(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction);
	bool obfuscate_inc(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction);
	bool convert_test(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction);
	bool obfuscate_call(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction);
	bool obfuscate_stack(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction);
	bool outline_instrs(std::vector<obfuscator::function_t>* functions, int maxOutline, int instrstrlen, bool xref);
public:

	obfuscator(pe64* pe);

	void create_functions(std::vector<pdbparser::sym_func>functions);

	void run(PIMAGE_SECTION_HEADER* new_section, GlobalArgs args);

	uint32_t get_added_size();

	struct instruction_t {

		int inst_id;
		int func_id;
		bool is_first_instruction;
		std::vector<uint8_t>raw_bytes;
		uint64_t runtime_address;
		uint64_t relocated_address;
		ZydisDecodedInstruction zyinstr;
		bool has_relative;
		bool isjmpcall;
		bool test_cmp = false;

		struct {
			int target_inst_id;
			int target_func_id;
			uint32_t offset;
			uint32_t size;
		}relative;

		uint64_t location_of_data;


		void load_relative_info();
		void load(int funcid, std::vector<uint8_t>raw_data);
		void load(int funcid, ZydisDecodedInstruction zyinstruction, uint64_t runtime_address);
		void reload();
		void print();
	};

	struct function_t {
		int func_id;
		std::string name;
		std::vector<instruction_t>instructions;
		uint32_t offset;
		uint32_t size;

		function_t(int func_id, std::string name, uint32_t offset, uint32_t size) : func_id(func_id), name(name), offset(offset), size(size) {};

		bool ctfflattening = true;
		bool movobf = true;
		bool addobf = true;
		bool leaobf = true;
		
		bool antidisassembly = true;
		bool popobf = true;
		bool retobf = true;
		bool xorobf = true;
		bool cmpobf = true;
		bool callobf = true;
		bool stackobf = true;
		bool incobf = true;
	};

	

	

};


