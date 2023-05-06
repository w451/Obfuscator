#include "../obfuscator.h"

ZydisRegister needpushRegsXor[] = { ZYDIS_REGISTER_RAX, ZYDIS_REGISTER_RCX,  ZYDIS_REGISTER_RDX, ZYDIS_REGISTER_R8, ZYDIS_REGISTER_R9, ZYDIS_REGISTER_R10, ZYDIS_REGISTER_R11};
ZydisRegister needpushRegsXor32[] = { ZYDIS_REGISTER_EAX, ZYDIS_REGISTER_ECX,  ZYDIS_REGISTER_EDX, ZYDIS_REGISTER_R8D, ZYDIS_REGISTER_R9D, ZYDIS_REGISTER_R10D, ZYDIS_REGISTER_R11D};

ZydisRegister getRegExcluding(ZydisRegister zr, int size) {
	ZydisRegister result = (ZydisRegister)0;
	do {
		if (size == 64) {
			result = ZYDIS_REGISTER_RAX;
		} else { //32
			result = ZYDIS_REGISTER_EAX;
		}
		result = (ZydisRegister)((int)result + (rand() % 16));
	} while (result == zr);
	return result;
}

bool obfuscator::obfuscate_xor(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {
	if (instruction->zyinstr.operands[0].reg.value == instruction->zyinstr.operands[1].reg.value) {
		if(instruction->zyinstr.operands[0].size==64 ||instruction->zyinstr.operands[0].size == 32) {
			if (rand()%2==0) {
				assm.crc32(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second);
				//assm.sub(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, 0);
				assm.xor_(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, 0); //Set flags appropriately
			} else {
				bool isRegClearedReg = instruction->zyinstr.operands[0].reg.value == ZYDIS_REGISTER_RDX || instruction->zyinstr.operands[0].reg.value == ZYDIS_REGISTER_EDX || instruction->zyinstr.operands[0].reg.value == ZYDIS_REGISTER_R10 || instruction->zyinstr.operands[0].reg.value == ZYDIS_REGISTER_R10D;
				ZydisRegister zr = rand() % 2 == 0 ? ZYDIS_REGISTER_EDX : ZYDIS_REGISTER_R10D;
				for (int i = 0; i < sizeof(needpushRegsXor) / 4; i++) {
					if (needpushRegsXor[i] != instruction->zyinstr.operands[0].reg.value && needpushRegsXor32[i] != instruction->zyinstr.operands[0].reg.value) {
						assm.push(lookupmap.find(needpushRegsXor[i])->second);
					}
				}

				assm.mov(lookupmap.find(ZYDIS_REGISTER_RAX)->second, getSyscallId());
				if (!isRegClearedReg) {
					if (rand()%2==0) {
						assm.mov(lookupmap.find(zr)->second, getRand64());
					} else {
						assm.mov(lookupmap.find(zr)->second, lookupmap.find(getRegExcluding(zr, instruction->zyinstr.operands[0].size))->second);
					}
				} else {
					if (rand() % 2 == 0) {
						assm.mov(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, getRand64());
					} else {
						assm.mov(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, lookupmap.find(getRegExcluding(instruction->zyinstr.operands[0].reg.value, instruction->zyinstr.operands[0].size))->second);
					}
				}
				assm.syscall();
				if (!isRegClearedReg) {
					assm.mov(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, lookupmap.find(zr)->second);
				}

				for (int i = sizeof(needpushRegsXor) / 4 - 1; i >= 0; i--) {
					if (needpushRegsXor[i] != instruction->zyinstr.operands[0].reg.value && needpushRegsXor32[i] != instruction->zyinstr.operands[0].reg.value) {
						assm.pop(lookupmap.find(needpushRegsXor[i])->second);
					}
				}
				//assm.xor_(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second);
				assm.xor_(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, 0); //Set flags appropriately
			}

		

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