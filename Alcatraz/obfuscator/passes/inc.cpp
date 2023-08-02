#include "../obfuscator.h"

#include <random>

using namespace asmjit;
bool obfuscator::obfuscate_inc(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {
	if (instruction->zyinstr.operands[0].reg.value!=ZYDIS_REGISTER_R14) {
		assm.push(x86::r14);
		assm.mov(x86::r14, 0xffffffffffffffff);
		assm.adcx(x86::r14, x86::r14);
		asmJitIsBad({0x72, 0xf8}, &assm);
		assm.add(x86::r14, 1);
		assm.adc(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, x86::r14);
		assm.pop(x86::r14);
	} else {
		assm.push(x86::r15);
		assm.mov(x86::r15, 0xffffffffffffffff);
		assm.adcx(x86::r15, x86::r15);
		asmJitIsBad({ 0x72, 0xf8 }, &assm);
		assm.add(x86::r15, 1);
		assm.adc(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, x86::r15);
		assm.pop(x86::r15);
	}

	void* fn = nullptr;
	auto err = rt.add(&fn, &code);

	auto jitinstructions = this->instructions_from_jit((uint8_t*)fn, code.codeSize());
	
	int orig_id = instruction->inst_id;
	instruction = function->instructions.erase(instruction);
	instruction -= 1;
	jitinstructions.at(0).inst_id = orig_id;
	jitinstructions.at(3).relative.target_inst_id = jitinstructions.at(2).inst_id;
	jitinstructions.at(3).relative.target_func_id = function->func_id;

	for (auto jit : jitinstructions) {
		instruction = function->instructions.insert(instruction + 1, jit);
	}

	code.reset();
	code.init(rt.environment());
	code.attach(&this->assm);
}