#include "../obfuscator.h"

bool obfuscator::obfuscate_return(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {
	//We have to hack around Zydis not wanting to dissassemble 4c 8d 15 00 00 00 00 (lea r10,[rip+0x0])

	std::vector<obfuscator::instruction_t> jitinstructions;
	if (rand()%2==0) {
		//R10 is considered volatile and should not be expected to hold data at a ret from a function
		assm.lea(lookupmap.find(ZYDIS_REGISTER_R10)->second, ptr(lookupmap.find(ZYDIS_REGISTER_R10)->second, 0x01000000)); //Use 0x01000000 in order to force use lea qword
		assm.add(lookupmap.find(ZYDIS_REGISTER_R10)->second, 6);
		assm.push(lookupmap.find(ZYDIS_REGISTER_R10)->second);
		assm.ret();

		void* fn = nullptr;
		auto err = rt.add(&fn, &code);

		jitinstructions = this->instructions_from_jit((uint8_t*)fn, code.codeSize());

		jitinstructions.at(0).zyinstr.operands[1].reg.value = ZYDIS_REGISTER_RIP; //Change the lea to lea r10,[rip+0x0]
		jitinstructions.at(0).zyinstr.operands->ptr.offset = 0;
		jitinstructions.at(0).raw_bytes.at(2) = 0x15;
		jitinstructions.at(0).raw_bytes.at(6) = 0;
		int orig_id = instruction->inst_id;
		instruction = function->instructions.erase(instruction);
		instruction -= 1;
		jitinstructions.at(0).inst_id = orig_id;

		for (auto jit : jitinstructions) {
			instruction = function->instructions.insert(instruction + 1, jit);
		}

	} else {
		instruction_t call0{};
		call0.load(function->func_id, { 0xE8, 0x00, 0x00, 0x00, 0x00 });
		call0.isjmpcall = false;
		call0.has_relative = false;
		instruction = function->instructions.insert(instruction, call0);
		instruction++;
	}


	
	code.reset();
	code.init(rt.environment());
	code.attach(&this->assm);
	return true;
}