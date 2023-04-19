#include "../obfuscator.h"

bool obfuscator::obfuscate_xor(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {
	bool activ = false;
	if (instruction->zyinstr.operands[0].reg.value == instruction->zyinstr.operands[1].reg.value) {
		if(instruction->zyinstr.operands[0].size==64) {
			if (instruction->zyinstr.operands[0].reg.value >= 53 && instruction->zyinstr.operands[0].reg.value <= 68 ) {
				ZydisRegister subreg = (ZydisRegister)(instruction->zyinstr.operands[0].reg.value - 16);
				assm.crc32(lookupmap.find(subreg)->second, lookupmap.find(subreg)->second);
				assm.shr(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, 32);
				assm.crc32(lookupmap.find(subreg)->second, lookupmap.find(subreg)->second);
				activ = true;
			}
		} else if (instruction->zyinstr.operands[0].size == 32) {
			assm.crc32(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second);
			assm.sub(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, 0);
			activ = true;
		}
		if (activ) {
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
	}
	return true;
}