#include "../obfuscator.h"

bool obfuscator::obfuscate_ff(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {

	instruction_t conditional_jmp{}; conditional_jmp.load(function->func_id, { 0xEB });
	conditional_jmp.isjmpcall = false;
	conditional_jmp.has_relative = false;
	instruction = function->instructions.insert(instruction, conditional_jmp);
	instruction++;

	return true;
}

bool obfuscator::add_junk(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {
	//This has a weird bug. Going to fix
	
	asmJitIsBad({0x51, 0xb9, 0x59, 0xeb, 0x04, 0x00, 0xeb, 0xfa}, &assm);

	void* fn = nullptr;
	auto err = rt.add(&fn, &code);
	auto jitinstructions = this->instructions_from_jit((uint8_t*)fn, code.codeSize());
	for (auto jit : jitinstructions) {
		jit.has_relative = false;
		jit.isjmpcall = false;
		instruction = function->instructions.insert(instruction + 1, jit);
	}

	instruction_t garbanzo{}; garbanzo.load(function->func_id, { (uint8_t)(rand()%255+1) });
	garbanzo.isjmpcall = false;
	garbanzo.has_relative = false;
	instruction = function->instructions.insert(instruction + 1, garbanzo);

	code.reset();
	code.init(rt.environment());
	code.attach(&this->assm);
	return true;

}