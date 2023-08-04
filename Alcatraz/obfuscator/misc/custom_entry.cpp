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
#define TYPE_REVERSE 4
#define TYPE_FAKE_EXITPOINT 5
#define TYPE_REAL_EXITPOINT 6


#define PREDICATE_BITS1 0
#define PREDICATE_BITS2 1

//Need to save RAX, RBX, RCX, RDX, R8, R9, R10
//Stored to    R10, R11, R12, R13, --, --, ---
//Usable: 
//Volatile: RBX, RCX, RDX
//EntryAddress: R14
//BaseAddress: R15

//Entry routine:
//mov R10, RAX
//mov R11, RBX
//mov R12, RCX
//mov R13, RDX
//mov R14, EncryptedEntryAddress
//mov R14, BaseAddress

//Exit routine:
// jmp to r14+r15

//Fake exit point:
//Always ends a block, just like true exit
//No predicate
//Must jump to the start of another block
//Do a address = "offset"
//Predict block offset via knowing size

//Previous seq is responsible for getting value to the target address
//FEP (and now ) perform XOR

long long SEQ_LEN = 1000;

typedef unsigned long long ULL;

typedef struct OPAQUEPREDICATE_ {
	int type;
	bool result;
	bool inverseJmp;
	Label target;
} OPAQUEPREDICATE;

typedef struct OBFSEQUENCE_ {
	int type;
	ULL offset;
	ULL val;
	bool jumps;
	Label thisLabel;
	OPAQUEPREDICATE predicate;
} OBFSEQUENCE;

typedef struct OBFBLOCK_ {
	ULL len = 0;
	ULL offset;
	Label thisLabel;
	struct OBFBLOCK_* nextBlock = nullptr;
	OBFSEQUENCE* seq = nullptr;
} OBFBLOCK;

ULL getSeqCodeSize(OBFSEQUENCE seq) {
	static constexpr int seqSizeByType[] = {18, 13, 7, 39, 3, 32, 32};
	static constexpr int opSizeByType[] = {25, 34};

	ULL size = seqSizeByType[seq.type];
	if (seq.type != TYPE_FAKE_EXITPOINT && seq.type != TYPE_REAL_EXITPOINT) {
		size += opSizeByType[seq.predicate.type] + 6; //6 -> jnz/jz
	}
	return size;
}

ULL getBlockCodeOffset(OBFBLOCK* block, ULL offset) {
	block->offset = offset;
	ULL size = 0; 
	for (ULL i = 0; i < block->len; i++) {
		block->seq[i].offset = offset + size;
		size += getSeqCodeSize(block->seq[i]);
	}
	return size;
}

OBFSEQUENCE* generateSeq(int len, x86::Assembler* assm) {
	OBFSEQUENCE* seq = (OBFSEQUENCE*)calloc(len + 1, sizeof(OBFSEQUENCE));
	for (long long i = 0; i < len; i++) {
		seq[i].thisLabel = assm->newLabel();
		seq[i].type = getRand64() % 6;
		seq[i].jumps = getRand64() % 2;

		if (i == len - 1) { //Last seq should always be the exit point
			seq[i].type = TYPE_REAL_EXITPOINT;
			seq[i].jumps = true;
			seq[i].val = getRand64() % ULLONG_MAX + 1;
		} else if (seq[i].type == TYPE_ROL) {
			seq[i].val = getRand64();
		} else if (seq[i].type == TYPE_XOR) {
			seq[i].val = getRand64();
		} else if (seq[i].type == TYPE_ROR_MAGIC) {
			seq[i].val = rand() % 2;
		} else if (seq[i].type == TYPE_ANTI_RECOMPILE) {
			seq[i].val = getRand64() % ULLONG_MAX + 1;
		} else if (seq[i].type == TYPE_FAKE_EXITPOINT) {
			seq[i].jumps = true;
		}
	}
	return seq;
}

Label createBlocks(OBFSEQUENCE* sequence, std::vector<OBFBLOCK*>* blocks, x86::Assembler* assm) {
	ULL blockLen = 0;
	for (long long i = 0; i < SEQ_LEN; i++) {
		blockLen++;
		if (sequence[i].jumps) {
			OBFBLOCK* newblock = new OBFBLOCK();
			newblock->len = blockLen;
			newblock->seq = &sequence[i - blockLen + 1];
			newblock->thisLabel = assm->newLabel();
			if (blocks->size() > 0) {
				blocks->at(blocks->size() - 1)->nextBlock = newblock;
			}
			blocks->push_back(newblock);
			blockLen = 0;
		}
	}

	Label firstLabel = blocks->at(0)->thisLabel;

	std::random_device rd;
	std::shuffle(std::begin(*blocks), std::end(*blocks), rd);
	return firstLabel;
}

OPAQUEPREDICATE createOpaquePredicate(Label target, bool result) {
	OPAQUEPREDICATE op = { 0 };
	op.result = result;
	op.type = getRand64() % 2;
	op.inverseJmp = getRand64() % 2;
	op.target = target;
	return op;
}

void generatePredicates(std::vector<OBFBLOCK*> blocks, OBFSEQUENCE* sequence) {
	for (long long i = 0; i < blocks.size(); i++) {
		OBFBLOCK* block = blocks.at(i);
		bool hasPredicate = block->seq[block->len - 1].type != TYPE_REAL_EXITPOINT && block->seq[block->len - 1].type != TYPE_FAKE_EXITPOINT;

		for (long long j = 0; j < block->len; j++) {
			OBFSEQUENCE obf = block->seq[j];
			if (hasPredicate || j!= block->len-1) {
				if (j != block->len - 1) {
					OPAQUEPREDICATE op = createOpaquePredicate(sequence[getRand64() % SEQ_LEN].thisLabel, false);
					block->seq[j].predicate = op;
				} else if (block->nextBlock != 0) {
					OPAQUEPREDICATE op = createOpaquePredicate(block->nextBlock->thisLabel, true);
					block->seq[j].predicate = op;
				}
			}
		}
	}
}

void compilePredicate(OPAQUEPREDICATE op, x86::Assembler* assm) {
	bool inverseJmp = op.inverseJmp;
	if (inverseJmp) {
		op.result = !op.result;
	}

	if (op.type == PREDICATE_BITS1) {
		ULL rnd = getRand64()%0xffffffffffffffff+1;

		assm->movabs(x86::rdx, rnd);
		assm->bsr(x86::rcx, x86::rdx);
		assm->or_(x86::rcx, 1);
		assm->shr(x86::rdx, x86::cl);

		unsigned long index = 0;
		_BitScanReverse64(&index, rnd);
		
		bool expecting = true;
		if (index % 2 == 0) {
			expecting = false;
		}
		
		if (!op.result) {
			expecting = !expecting;
		}
		assm->cmp(x86::rdx, expecting);
	} else if (op.type == PREDICATE_BITS2) {
		ULL rnd = getRand64() % 0xffffffffffffffff + 1;


		ULL rndshr = rnd;
		ULL rndror = rnd;
		int expresult = 0;
		do {
			expresult++;
			rndshr = rndshr >> 1;
			rndror = _rotr64(rndror, 1);
		} while (rndshr == rndror);

		if (!op.result) {
			int answer = expresult;
			do {
				expresult = getRand64() % 16 + 1;
			} while (expresult == answer);
		}

		assm->movabs(x86::rdx, rnd);
		assm->mov(x86::rcx, x86::rdx);
		assm->xor_(x86::rbx, x86::rbx);
		Label l1 = assm->newLabel();
		assm->bind(l1);
		assm->inc(x86::rbx);
		assm->shr(x86::rdx, 1);
		assm->ror(x86::rcx, 1);
		assm->cmp(x86::rdx, x86::rcx);
		assm->jz(l1);
		assm->cmp(x86::rbx, expresult);
	}

	if (inverseJmp) {
		assm->long_().jnz(op.target);
	} else {
		assm->long_().jz(op.target);
	}
}

void obfuscator::add_custom_entry(PIMAGE_SECTION_HEADER* new_section, long long count) {
	SEQ_LEN = count;
	if (pe->get_path().find(".exe") != std::string::npos) {
		ULL entryAddress = pe->get_nt()->OptionalHeader.AddressOfEntryPoint;
		ULL sectionOffset = (*new_section)->VirtualAddress + this->total_size_used;

		std::cout << "Goal entry address: " << std::hex << entryAddress <<" sec: "<< sectionOffset << std::endl;
		OBFSEQUENCE* sequence = generateSeq(SEQ_LEN, &assm);
		std::vector<OBFBLOCK*> blocks;
		Label firstLabel = createBlocks(sequence, &blocks, &assm);
		generatePredicates(blocks, sequence);


		std::cout << std::dec << blocks.size()<< " blocks" << std::endl;
		ULL estSize = 0;
		estSize += 40;
		for (OBFBLOCK* b : blocks) {
			estSize += getBlockCodeOffset(b, estSize);
		}
		estSize += 1;

		std::cout << "Estimated: " << std::hex << estSize << " bytes" << std::endl;

		for (long long i = SEQ_LEN - 1; i >= 0; i--) {
			OBFSEQUENCE obf = sequence[i];
			if (obf.type == TYPE_ROL) {
				entryAddress = _rotr64(entryAddress, __popcnt64(obf.val));
			} else if (obf.type == TYPE_XOR) {
				entryAddress = entryAddress ^ obf.val;
			} else if (obf.type == TYPE_ROR_MAGIC) {
				entryAddress = _rotl64(entryAddress, pe->get_buffer()->at(obf.val));
			} else if (obf.type == TYPE_REVERSE) {
				entryAddress = _byteswap_uint64(entryAddress);
			} else if (obf.type == TYPE_FAKE_EXITPOINT) {
				sequence[i].val = entryAddress ^ (sequence[i + 1].offset + sectionOffset);
				entryAddress = sequence[i + 1].offset + sectionOffset;
			} else if (obf.type == TYPE_REAL_EXITPOINT) {
				sequence[i].val = getRand64(); //Make it look like we are continuing
			}
		}

		assm.mov(x86::r10, x86::rax);
		assm.mov(x86::r11, x86::rbx);
		assm.mov(x86::r12, x86::rcx);
		assm.mov(x86::r13, x86::rdx);
		assm.embedUInt64(0x00006025048b4865); //mov rax,QWORD PTR gs:0x60
		assm.embedUInt8(0x00);
		assm.mov(lookupmap.find(ZYDIS_REGISTER_R15)->second, ptr(lookupmap.find(ZYDIS_REGISTER_RAX)->second, 0x10));
		assm.movabs(lookupmap.find(ZYDIS_REGISTER_R14)->second, entryAddress);

		assm.jmp(firstLabel);
		for (long long i = 0; i < blocks.size(); i++) {
			OBFBLOCK block = *blocks.at(i);
			assm.bind(block.thisLabel);
			bool hasPredicate = block.seq[block.len - 1].type != TYPE_REAL_EXITPOINT && block.seq[block.len - 1].type != TYPE_FAKE_EXITPOINT;

			for (long long j = 0; j < block.len; j++) {
				OBFSEQUENCE obf = block.seq[j];
				assm.bind(obf.thisLabel);
				if (obf.type == TYPE_ROL) {
					assm.movabs(lookupmap.find(ZYDIS_REGISTER_RCX)->second, obf.val);
					assm.popcnt(lookupmap.find(ZYDIS_REGISTER_RCX)->second, lookupmap.find(ZYDIS_REGISTER_RCX)->second);
					assm.rol(lookupmap.find(ZYDIS_REGISTER_R14)->second, lookupmap.find(ZYDIS_REGISTER_CL)->second);
				} else if (obf.type == TYPE_XOR) {
					assm.movabs(lookupmap.find(ZYDIS_REGISTER_RCX)->second, obf.val);
					assm.xor_(lookupmap.find(ZYDIS_REGISTER_R14)->second, lookupmap.find(ZYDIS_REGISTER_RCX)->second);
				} else if (obf.type == TYPE_ROR_MAGIC) {
					asmJitIsBad({0x41, 0x8a, 0x4f, (byte) obf.val}, &assm);//mov cl, byte ptr [r15+obf.val]
					assm.ror(lookupmap.find(ZYDIS_REGISTER_R14)->second, lookupmap.find(ZYDIS_REGISTER_CL)->second);
				} else if (obf.type == TYPE_ANTI_RECOMPILE) {
					assm.push(lookupmap.find(ZYDIS_REGISTER_RAX)->second);
					assm.push(lookupmap.find(ZYDIS_REGISTER_R11)->second);
					assm.push(lookupmap.find(ZYDIS_REGISTER_R13)->second);
					assm.push(lookupmap.find(ZYDIS_REGISTER_R14)->second);
					assm.movabs(lookupmap.find(ZYDIS_REGISTER_RDX)->second, obf.val);
					assm.movabs(lookupmap.find(ZYDIS_REGISTER_RAX)->second, getSyscallId());
					assm.syscall();
					assm.pop(lookupmap.find(ZYDIS_REGISTER_R14)->second);
					assm.pop(lookupmap.find(ZYDIS_REGISTER_R13)->second);
					assm.pop(lookupmap.find(ZYDIS_REGISTER_R11)->second);
					assm.pop(lookupmap.find(ZYDIS_REGISTER_RAX)->second);
					assm.add(lookupmap.find(ZYDIS_REGISTER_R14)->second, lookupmap.find(ZYDIS_REGISTER_RDX)->second);
				} else if (obf.type == TYPE_REVERSE) {
					assm.bswap(x86::r14);
				} else if (obf.type == TYPE_FAKE_EXITPOINT) {
					assm.push(x86::r15);
					assm.add(x86::qword_ptr(x86::rsp), x86::r14);
					assm.movabs(lookupmap.find(ZYDIS_REGISTER_RCX)->second, obf.val);
					assm.xor_(lookupmap.find(ZYDIS_REGISTER_R14)->second, lookupmap.find(ZYDIS_REGISTER_RCX)->second);
					assm.mov(x86::rax, x86::r10);
					assm.mov(x86::rbx, x86::r11);
					assm.mov(x86::rcx, x86::r12);
					assm.mov(x86::rdx, x86::r13);
					assm.ret();
				} else if (obf.type == TYPE_REAL_EXITPOINT) {
					assm.push(x86::r15);
					assm.add(x86::qword_ptr(x86::rsp), x86::r14);
					assm.movabs(lookupmap.find(ZYDIS_REGISTER_RCX)->second, obf.val);
					assm.xor_(lookupmap.find(ZYDIS_REGISTER_R14)->second, lookupmap.find(ZYDIS_REGISTER_RCX)->second);
					assm.mov(x86::rax, x86::r10);
					assm.mov(x86::rbx, x86::r11);
					assm.mov(x86::rcx, x86::r12);
					assm.mov(x86::rdx, x86::r13);
					assm.ret();
				}
				if (hasPredicate||j!=block.len-1) {
					compilePredicate(obf.predicate, &assm);
				}
			}
		}
		assm.ret(); //This will never run but we are being very nice to decompilers :D

		free(sequence);
		for (OBFBLOCK* obfp : blocks) {
			free(obfp);
		}

		void* fn = nullptr;
		asmjit::Error err = rt.add(&fn, &code);
		std::cout << "Assembled " << std::hex << code.codeSize() << " bytes" << std::endl;

		uint64_t offset = (uint64_t)(*new_section) - (uint64_t)pe->get_buffer()->data();
		pe->get_buffer()->resize(pe->get_buffer()->capacity() + code.codeSize());
		*new_section = (PIMAGE_SECTION_HEADER)(offset + pe->get_buffer()->data());
		void* address = (void*)(pe->get_buffer()->data() + (*new_section)->VirtualAddress + this->total_size_used);

		memcpy(address, fn, code.codeSize());

		this->total_size_used += code.codeSize();
		pe->get_nt()->OptionalHeader.SizeOfCode += code.codeSize();
		pe->get_nt()->OptionalHeader.AddressOfEntryPoint = (uint64_t)address - (uint64_t)pe->get_buffer()->data();

		code.reset();
		code.init(rt.environment());
		code.attach(&this->assm);

		/*void* fn = nullptr;
		auto err = rt.add(&fn, &code);
		std::cout << "Pre cheb code: "<<code.codeSize() << std::endl;
		std::vector<obfuscator::instruction_t> jitinstructions = this->instructions_from_jit((uint8_t*)fn, code.codeSize());
		std::cout << "instrs: "<<  jitinstructions.size() << std::endl;
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
		code.attach(&this->assm);*/
	} else if (pe->get_path().find(".dll") != std::string::npos) {
		throw std::runtime_error("File type doesn't support custom entry!\n");
	} else if (pe->get_path().find(".sys") != std::string::npos) {
		throw std::runtime_error("File type doesn't support custom entry!\n");
	} else
		throw std::runtime_error("File type doesn't support custom entry!\n");
}