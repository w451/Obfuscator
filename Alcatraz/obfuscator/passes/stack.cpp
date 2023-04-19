#include "../obfuscator.h"

bool makeRip(std::vector<obfuscator::instruction_t>* jitinstructions, int n) {
	jitinstructions->at(1).zyinstr.operands[1].reg.value = ZYDIS_REGISTER_RIP; //Change the lea to lea r10,[rip+0x0]
	jitinstructions->at(1).zyinstr.operands->ptr.offset = 0;
	jitinstructions->at(1).raw_bytes.at(2) = 0x3d;
	jitinstructions->at(1).raw_bytes.at(6) = 0;

	jitinstructions->at(3 + n).zyinstr.operands[1].reg.value = ZYDIS_REGISTER_RIP; //Change the lea to lea r10,[rip+0x0]
	jitinstructions->at(3 + n).zyinstr.operands->ptr.offset = 0;
	jitinstructions->at(3 + n).raw_bytes.at(2) = 0x3d;
	jitinstructions->at(3 + n).raw_bytes.at(6) = 0;

}

bool obfuscator::obfuscate_stack(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {
	if (instruction->zyinstr.operands[1].imm.value.s >= 10) {
		int nopcount = instruction->zyinstr.operands[1].imm.value.s - 10;

	

		if (instruction->zyinstr.mnemonic == ZYDIS_MNEMONIC_ADD) {
			assm.mov(ptr(lookupmap.find(ZYDIS_REGISTER_RSP)->second, 0), lookupmap.find(ZYDIS_REGISTER_R15)->second);
			assm.lea(lookupmap.find(ZYDIS_REGISTER_R15)->second, ptr(lookupmap.find(ZYDIS_REGISTER_R10)->second, 0x01000000));
			assm.sub(lookupmap.find(ZYDIS_REGISTER_RSP)->second, lookupmap.find(ZYDIS_REGISTER_R15)->second); //3 bytes
			for (int i = 0; i < nopcount; i++) {
				assm.nop();
			}
			assm.lea(lookupmap.find(ZYDIS_REGISTER_R15)->second, ptr(lookupmap.find(ZYDIS_REGISTER_R10)->second, 0x01000000)); //7 bytes
			assm.add(lookupmap.find(ZYDIS_REGISTER_RSP)->second, lookupmap.find(ZYDIS_REGISTER_R15)->second);
			assm.mov(lookupmap.find(ZYDIS_REGISTER_R15)->second, ptr(lookupmap.find(ZYDIS_REGISTER_RSP)->second, -instruction->zyinstr.operands[1].imm.value.s));
		} else { //sub
			assm.mov(ptr(lookupmap.find(ZYDIS_REGISTER_RSP)->second, -8), lookupmap.find(ZYDIS_REGISTER_R15)->second);
			assm.lea(lookupmap.find(ZYDIS_REGISTER_R15)->second, ptr(lookupmap.find(ZYDIS_REGISTER_R10)->second, 0x01000000));
			assm.add(lookupmap.find(ZYDIS_REGISTER_RSP)->second, lookupmap.find(ZYDIS_REGISTER_R15)->second); //3 bytes
			for (int i = 0; i < nopcount; i++) {
				assm.nop();
			}
			assm.lea(lookupmap.find(ZYDIS_REGISTER_R15)->second, ptr(lookupmap.find(ZYDIS_REGISTER_R10)->second, 0x01000000)); //7 bytes
			assm.sub(lookupmap.find(ZYDIS_REGISTER_RSP)->second, lookupmap.find(ZYDIS_REGISTER_R15)->second);
			assm.mov(lookupmap.find(ZYDIS_REGISTER_R15)->second, 0);
			assm.xchg(lookupmap.find(ZYDIS_REGISTER_R15)->second, ptr(lookupmap.find(ZYDIS_REGISTER_RSP)->second, instruction->zyinstr.operands[1].imm.value.s-8));
		}

		

		void* fn = nullptr;
		auto err = rt.add(&fn, &code);

		auto jitinstructions = this->instructions_from_jit((uint8_t*)fn, code.codeSize());
		makeRip(&jitinstructions, nopcount);


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