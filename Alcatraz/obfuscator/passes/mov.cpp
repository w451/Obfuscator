#include "../obfuscator.h"
#include <random>
#include <vector>

ZydisRegister needpushRegsMov[] = { ZYDIS_REGISTER_RAX, ZYDIS_REGISTER_RCX,  ZYDIS_REGISTER_RDX, ZYDIS_REGISTER_R8, ZYDIS_REGISTER_R9, ZYDIS_REGISTER_R10, ZYDIS_REGISTER_R11, ZYDIS_REGISTER_RBP};
ZydisRegister needpushRegsMov32[] = { ZYDIS_REGISTER_EAX, ZYDIS_REGISTER_ECX,  ZYDIS_REGISTER_EDX, ZYDIS_REGISTER_R8D, ZYDIS_REGISTER_R9D, ZYDIS_REGISTER_R10D, ZYDIS_REGISTER_R11D, ZYDIS_REGISTER_EBP };

bool obfuscator::obfuscate_mov(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {

	if (instruction->zyinstr.operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && instruction->zyinstr.operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {

		auto x86_register_map = lookupmap.find(instruction->zyinstr.operands[0].reg.value);

		if (x86_register_map != lookupmap.end()) {

			int bit_of_value = instruction->zyinstr.raw.imm->size;

			auto usingregister = x86_register_map->second;

			//If the datatype doesnt match the register we skip it due to our rotations. You could translate to that register but meh
			if (bit_of_value == 8 && !usingregister.isGpb())
				return false;

			if (bit_of_value == 16 && !usingregister.isGpw())
				return false;

			if (bit_of_value == 32 && !usingregister.isGpd())
				return false;

			if (bit_of_value == 64 && !usingregister.isGpq())
				return false;


			std::random_device rd;
			std::default_random_engine generator(rd());

			uint32_t random_add_val, rand_xor_val, rand_rot_val;
			switch (bit_of_value) {
			case 8: {
				random_add_val = rand() % 255 + 1;
				rand_xor_val = rand() % 255 + 1;
				rand_rot_val = rand() % 255 + 1;
				*(uint8_t*)(&instruction->raw_bytes.data()[instruction->zyinstr.raw.imm->offset]) = ~((_rotr8(*(uint8_t*)(&instruction->raw_bytes.data()[instruction->zyinstr.raw.imm->offset]), rand_rot_val) ^ rand_xor_val) - random_add_val);
				break;
			}
			case 16: {
				std::uniform_int_distribution<uint16_t>distribution(INT16_MAX / 2, INT16_MAX);
				random_add_val = distribution(generator);
				rand_xor_val = distribution(generator);
				rand_rot_val = distribution(generator);
				*(uint16_t*)(&instruction->raw_bytes.data()[instruction->zyinstr.raw.imm->offset]) = ~((_rotr16(*(uint16_t*)(&instruction->raw_bytes.data()[instruction->zyinstr.raw.imm->offset]), rand_rot_val) ^ rand_xor_val) - random_add_val);
				break;
			}
			case 32: {
				std::uniform_int_distribution<uint32_t>distribution(UINT32_MAX / 2, UINT32_MAX);
				random_add_val = distribution(generator);
				rand_xor_val = distribution(generator);
				rand_rot_val = distribution(generator);
				*(uint32_t*)(&instruction->raw_bytes.data()[instruction->zyinstr.raw.imm->offset]) = ~((_rotr(*(uint32_t*)(&instruction->raw_bytes.data()[instruction->zyinstr.raw.imm->offset]), rand_rot_val) ^ rand_xor_val) - random_add_val);
				break;
			}
			case 64: {
				std::uniform_int_distribution<uint32_t>distribution(INT32_MAX / 2, INT32_MAX);
				random_add_val = distribution(generator);
				rand_xor_val = distribution(generator);
				rand_rot_val = distribution(generator);
				*(uint64_t*)(&instruction->raw_bytes.data()[instruction->zyinstr.raw.imm->offset]) = ~((_rotr64(*(uint64_t*)(&instruction->raw_bytes.data()[instruction->zyinstr.raw.imm->offset]), rand_rot_val) ^ rand_xor_val) - random_add_val);
				break;
			}
			}

			assm.pushf();
			assm.not_(usingregister);
			assm.add(usingregister, random_add_val);
			assm.xor_(usingregister, rand_xor_val);
			assm.rol(usingregister, rand_rot_val);
			assm.popf();

			void* fn = nullptr;
			auto err = rt.add(&fn, &code);

			auto jitinstructions = this->instructions_from_jit((uint8_t*)fn, code.codeSize());
			for (auto jit : jitinstructions) {
				instruction = function->instructions.insert(instruction + 1, jit);
			}

			code.reset();
			code.init(rt.environment());
			code.attach(&this->assm);
			return true;

		}
	
	

	} else if (instruction->zyinstr.operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && instruction->zyinstr.operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER
		&& instruction->zyinstr.operands[0].reg.value != ZYDIS_REGISTER_RSP && instruction->zyinstr.operands[0].reg.value != ZYDIS_REGISTER_ESP
		&& instruction->zyinstr.operands[1].reg.value != ZYDIS_REGISTER_RSP && instruction->zyinstr.operands[1].reg.value != ZYDIS_REGISTER_ESP) {
		if (instruction->zyinstr.operands[0].size == 64) {	// BUG WITH mov RCX, RBX !!
			/*assm.pushf();
			for (int i = 0; i < sizeof(needpushRegsMov) / 4; i++) {
				if (needpushRegsMov[i] != instruction->zyinstr.operands[0].reg.value) {
					assm.push(lookupmap.find(needpushRegsMov[i])->second);
				}
			}

			if (instruction->zyinstr.operands[1].reg.value != ZYDIS_REGISTER_RBP) {
				assm.mov(lookupmap.find(ZYDIS_REGISTER_RBP)->second, lookupmap.find(instruction->zyinstr.operands[1].reg.value)->second);
			}
			assm.mov(lookupmap.find(ZYDIS_REGISTER_RAX)->second, getSyscallId());
			//assm.mov(lookupmap.find(ZYDIS_REGISTER_R9)->second, getRand64());
			assm.syscall();
			if (instruction->zyinstr.operands[0].reg.value != ZYDIS_REGISTER_R9) {
				assm.mov(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, lookupmap.find(ZYDIS_REGISTER_R9)->second);
			}
			for (int i = sizeof(needpushRegsMov) / 4 - 1; i >= 0; i--) {
				if (needpushRegsMov[i] != instruction->zyinstr.operands[0].reg.value) {
					assm.pop(lookupmap.find(needpushRegsMov[i])->second);
				}
			}
			assm.popf();

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
			code.attach(&this->assm);*/
		} else if (instruction->zyinstr.operands[0].size == 32) {
			/*assm.pushf();
			for (int i = 0; i < sizeof(needpushRegsMov) / 4; i++) {
				if (needpushRegsMov32[i] != instruction->zyinstr.operands[0].reg.value) {
					assm.push(lookupmap.find(needpushRegsMov[i])->second);
				}
			}

			if (instruction->zyinstr.operands[1].reg.value != ZYDIS_REGISTER_EBP) {
				assm.mov(lookupmap.find(ZYDIS_REGISTER_EBP)->second, lookupmap.find(instruction->zyinstr.operands[1].reg.value)->second);
			}
			assm.mov(lookupmap.find(ZYDIS_REGISTER_RAX)->second, getSyscallId());
			assm.mov(lookupmap.find(ZYDIS_REGISTER_R9D)->second, (ULONG32)getRand64());
			assm.syscall();
			if (instruction->zyinstr.operands[0].reg.value != ZYDIS_REGISTER_R9D) {
				assm.mov(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, lookupmap.find(ZYDIS_REGISTER_R9D)->second);
			}
			for (int i = sizeof(needpushRegsMov) / 4 - 1; i >= 0; i--) {
				if (needpushRegsMov32[i] != instruction->zyinstr.operands[0].reg.value) {
					assm.pop(lookupmap.find(needpushRegsMov[i])->second);
				}
			}
			assm.cmp(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, lookupmap.find(instruction->zyinstr.operands[1].reg.value)->second);
			//assm.jz(1);
			assm.int3();
			assm.popf();

			void* fn = nullptr;
			auto err = rt.add(&fn, &code);
			auto jitinstructions = this->instructions_from_jit((uint8_t*)fn, code.codeSize());
			int orig_id = instruction->inst_id;
			instruction = function->instructions.erase(instruction);
			instruction -= 1;
			jitinstructions.at(0).inst_id = orig_id;
			int i = 0;
			for (auto jit : jitinstructions) {
				if (i == jitinstructions.size()-2) {
					instruction_t jz1 = { 0 }; jz1.load(function->func_id, { 0x0f, 0x84, 0x01, 0x00,0x00,0x00 });
					jz1.has_relative = true;
					jz1.relative.offset = 2;
					jz1.relative.size = 32;
					jz1.relative.target_func_id = function->func_id;
					jitinstructions.at(jitinstructions.size() - 1).func_id = function->func_id;
					jz1.relative.target_inst_id = jitinstructions.at(jitinstructions.size() - 1).inst_id;
					instruction = function->instructions.insert(instruction + 1, jz1);
				}
				instruction = function->instructions.insert(instruction + 1, jit);
				i++;
			}
			instruction->test_cmp = true;

			code.reset();
			code.init(rt.environment());
			code.attach(&this->assm);*/
		}
	}

	return false;
}