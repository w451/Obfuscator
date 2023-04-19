#include "../obfuscator.h"



/*bool obfuscator::convert_test(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {
	if (instruction->zyinstr.operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER
		&& instruction->zyinstr.operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER
		&& instruction->zyinstr.operands[0].reg.value == instruction->zyinstr.operands[1].reg.value) {

		assm.cmp(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, 0);

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
		instruction->test_cmp = true;

		code.reset();
		code.init(rt.environment());
		code.attach(&this->assm);
	}
}*/

bool obfuscator::convert_test(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {
	return true;
}