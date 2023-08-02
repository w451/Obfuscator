#include "../obfuscator.h"
#include <stdexcept>
#include <intrin.h>
#include <algorithm>
#include <random>
#include <iostream>

DWORD insertedId;

#define TYPE_ROL 0
#define TYPE_XOR 1
#define TYPE_ROR_MAGIC 2
#define TYPE_ANTI_RECOMPILE 3
#define TYPE_REAL_EXITPOINT 4

ZydisRegister pushRs[] = { ZYDIS_REGISTER_RDX, ZYDIS_REGISTER_R8, ZYDIS_REGISTER_R9, ZYDIS_REGISTER_R10, ZYDIS_REGISTER_R11 };

constexpr long long SEQ_LEN = 10;

typedef unsigned long long ULL;

typedef struct OBFSEQUENCE_ {
	int type;
	ULONG64 val;
	bool jumps;
} OBFSEQUENCE;

typedef struct OBFBLOCK_ {
	ULL len;
	Label thisLabel;
	Label targetLabel;
	OBFSEQUENCE* seq;
} OBFBLOCK;

//(2x+1)(x+1)(x) < ULLMAX when x <= 2097151

OBFSEQUENCE* generateSeq(int len) {
	OBFSEQUENCE* seq = (OBFSEQUENCE*)calloc(len + 1, sizeof(OBFSEQUENCE));
	for (long long i = 0; i < len; i++) {
		seq[i].type = getRand64() % 4;
		seq[i].jumps = getRand64() % 2;

		if (i == len - 1) { //Last seq should always be the exit point
			seq[i].type = TYPE_REAL_EXITPOINT;
			seq[i].jumps = true;
		} else if (seq[i].type == TYPE_ROL) {
			seq[i].val = getRand64();
		} else if (seq[i].type == TYPE_XOR) {
			seq[i].val = getRand64();
		} else if (seq[i].type == TYPE_ROR_MAGIC) {
			seq[i].val = rand() % 2;
		} else if (seq[i].type == TYPE_ANTI_RECOMPILE) {
			seq[i].val = getRand64() % ULLONG_MAX + 1;
		}
	}
	return seq;
}


Label createBlocks(OBFSEQUENCE* sequence, std::vector<OBFBLOCK>* blocks, x86::Assembler* assm) {
	ULL blockLen = 0;
	for (long long i = 0; i < SEQ_LEN; i++) {
		blockLen++;
		if (sequence[i].jumps) {
			OBFBLOCK newblock = { 0 };
			newblock.len = blockLen;
			newblock.seq = &sequence[i - blockLen + 1];
			newblock.thisLabel = assm->newLabel();
			if (blocks->size() > 0) {
				blocks->at(blocks->size() - 1).targetLabel = newblock.thisLabel;
			}
			blocks->push_back(newblock);
			blockLen = 0;
		}
	}

	Label firstLabel = blocks->at(0).thisLabel;

	std::random_device rd;
	std::shuffle(std::begin(*blocks), std::end(*blocks), rd);
	return firstLabel;
}


void obfuscator::add_custom_entry(PIMAGE_SECTION_HEADER* new_section) {
	if (pe->get_path().find(".exe") != std::string::npos) {
		ULONG64 entryAddress = pe->get_nt()->OptionalHeader.AddressOfEntryPoint;

		OBFSEQUENCE* sequence = generateSeq(SEQ_LEN);
		for (long long i = SEQ_LEN - 1; i >= 0; i--) {
			OBFSEQUENCE obf = sequence[i];
			if (obf.type == TYPE_ROL) {
				entryAddress = _rotr64(entryAddress, __popcnt64(obf.val));
			} else if (obf.type == TYPE_XOR) {
				entryAddress = entryAddress ^ obf.val;
			} else if (obf.type == TYPE_ROR_MAGIC) {
				entryAddress = _rotl64(entryAddress, pe->get_buffer()->at(obf.val));
			}
		}

		std::vector<OBFBLOCK> blocks;
		Label firstLabel = createBlocks(sequence, &blocks, &assm);
		std::cout << blocks.size()<< " blocks" << std::endl;


		assm.push(lookupmap.find(ZYDIS_REGISTER_RAX)->second);
		assm.pushf();
		assm.push(lookupmap.find(ZYDIS_REGISTER_RBX)->second);
		assm.push(lookupmap.find(ZYDIS_REGISTER_RCX)->second);
		assm.embedUInt64(0x00006025048b4865); //mov rax,QWORD PTR gs:0x60
		assm.embedUInt8(0x00);
		assm.mov(lookupmap.find(ZYDIS_REGISTER_RAX)->second, ptr(lookupmap.find(ZYDIS_REGISTER_RAX)->second, 0x10));
		assm.mov(lookupmap.find(ZYDIS_REGISTER_RBX)->second, entryAddress);

		for (int i = 0; i < sizeof(pushRs) / sizeof(ZydisRegister); i++) {
			assm.push(lookupmap.find(pushRs[i])->second);
		}

		assm.jmp(firstLabel);

		for (long long i = 0; i < blocks.size(); i++) {
			assm.bind(blocks.at(i).thisLabel);
			for (long long j = 0; j < blocks.at(i).len; j++) {

				OBFSEQUENCE obf = blocks.at(i).seq[j];
				if (obf.type == TYPE_ROL) {
					assm.mov(lookupmap.find(ZYDIS_REGISTER_RCX)->second, obf.val);
					assm.popcnt(lookupmap.find(ZYDIS_REGISTER_RCX)->second, lookupmap.find(ZYDIS_REGISTER_RCX)->second);
					assm.rol(lookupmap.find(ZYDIS_REGISTER_RBX)->second, lookupmap.find(ZYDIS_REGISTER_CL)->second);
				} else if (obf.type == TYPE_XOR) {
					assm.mov(lookupmap.find(ZYDIS_REGISTER_RCX)->second, obf.val);
					assm.xor_(lookupmap.find(ZYDIS_REGISTER_RBX)->second, lookupmap.find(ZYDIS_REGISTER_RCX)->second);
				} else if (obf.type == TYPE_ROR_MAGIC) {
					assm.mov(lookupmap.find(ZYDIS_REGISTER_CL)->second, x86::byte_ptr(lookupmap.find(ZYDIS_REGISTER_RAX)->second, obf.val));
					assm.ror(lookupmap.find(ZYDIS_REGISTER_RBX)->second, lookupmap.find(ZYDIS_REGISTER_CL)->second);
				} else if (obf.type == TYPE_ANTI_RECOMPILE) {
					assm.push(lookupmap.find(ZYDIS_REGISTER_RAX)->second);
					assm.mov(lookupmap.find(ZYDIS_REGISTER_RDX)->second, obf.val);
					assm.mov(lookupmap.find(ZYDIS_REGISTER_RAX)->second, getSyscallId());
					assm.syscall();
					assm.pop(lookupmap.find(ZYDIS_REGISTER_RAX)->second);
					assm.add(lookupmap.find(ZYDIS_REGISTER_RBX)->second, lookupmap.find(ZYDIS_REGISTER_RDX)->second);
				} else if (obf.type == TYPE_REAL_EXITPOINT) {
					for (int i = sizeof(pushRs) / sizeof(ZydisRegister) - 1; i >= 0; i--) {
						assm.pop(lookupmap.find(pushRs[i])->second);
					}
					assm.add(lookupmap.find(ZYDIS_REGISTER_RAX)->second, lookupmap.find(ZYDIS_REGISTER_RBX)->second);
					assm.pop(lookupmap.find(ZYDIS_REGISTER_RCX)->second);
					assm.pop(lookupmap.find(ZYDIS_REGISTER_RBX)->second);
					assm.popf();
					assm.xchg(lookupmap.find(ZYDIS_REGISTER_RAX)->second, qword_ptr(lookupmap.find(ZYDIS_REGISTER_RSP)->second, 0));
					assm.ret();
				}
				//Fake jmp out here based on FALSE opaque predicate if not last part of a block
				//jump to random OBFSEQUENCE using label list 
			}
			if (blocks.at(i).targetLabel.id() != Globals::kInvalidId) { //Use TRUE opaque predicate to actually jump out
				assm.jmp(blocks.at(i).targetLabel);
			}
		}


		free(sequence);
		
		void* fn = nullptr;
		auto err = rt.add(&fn, &code);
		std::vector<obfuscator::instruction_t> jitinstructions = this->instructions_from_jit((uint8_t*)fn, code.codeSize());
		uint64_t offset = (uint64_t)(*new_section) - (uint64_t)pe->get_buffer()->data();
		pe->get_buffer()->resize(pe->get_buffer()->capacity() + jitinstructions.size() * 0xf);
		*new_section = (PIMAGE_SECTION_HEADER)(offset + pe->get_buffer()->data());

		for (auto inst = jitinstructions.begin(); inst != jitinstructions.end(); ++inst) {
			inst->isjmpcall = false;
			inst->has_relative = false;
			void* address = (void*)(pe->get_buffer()->data() + (*new_section)->VirtualAddress + this->total_size_used);

			inst->relocated_address = (uint64_t)address;
			memcpy(address, inst->raw_bytes.data(), inst->zyinstr.length);
			this->total_size_used += inst->zyinstr.length;
			pe->get_nt()->OptionalHeader.SizeOfCode += inst->zyinstr.length;
		}

		pe->get_nt()->OptionalHeader.AddressOfEntryPoint = jitinstructions.at(0).relocated_address - (uint64_t)pe->get_buffer()->data();
		code.reset();
		code.init(rt.environment());
		code.attach(&this->assm);
	} else if (pe->get_path().find(".dll") != std::string::npos) {
		throw std::runtime_error("File type doesn't support custom entry!\n");
	} else if (pe->get_path().find(".sys") != std::string::npos) {
		throw std::runtime_error("File type doesn't support custom entry!\n");
	} else
		throw std::runtime_error("File type doesn't support custom entry!\n");
}