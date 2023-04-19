#include "../obfuscator.h"


bool obfuscator::obfuscate_pop(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {
	if (instruction->zyinstr.operands[0].reg.value == ZYDIS_REGISTER_RAX) {
		assm.xchg(lookupmap.find(ZYDIS_REGISTER_RSI)->second, lookupmap.find(ZYDIS_REGISTER_RSP)->second);
		assm.lodsq();
		assm.xchg(lookupmap.find(ZYDIS_REGISTER_RSI)->second, lookupmap.find(ZYDIS_REGISTER_RSP)->second);

		void* fn = nullptr;
		auto err = rt.add(&fn, &code);

		auto jitinstructions = this->instructions_from_jit((uint8_t*)fn, code.codeSize());
		int orig_id = instruction->inst_id;
		instruction = function->instructions.erase(instruction);
		instruction -= 1;
		jitinstructions.at(0).inst_id = orig_id;

		for (auto jit : jitinstructions) {
			instruction = function->instructions.insert(instruction + 1, jit);			
		}

		code.reset();
		code.init(rt.environment());
		code.attach(&this->assm);
	}
	return true;
}