#include "../obfuscator.h"
#include <stdexcept>
#include <intrin.h>

DWORD insertedId;

#define TYPE_ROL 0
#define TYPE_XOR 1
#define TYPE_ROR_MAGIC 2
#define TYPE_MANIP_SYSCALL 3
#define TYPE_OPAQUE 4

ZydisRegister pushRs[] = { ZYDIS_REGISTER_RDX, ZYDIS_REGISTER_R8, ZYDIS_REGISTER_R9, ZYDIS_REGISTER_R10, ZYDIS_REGISTER_R11 };

typedef struct OBFSEQUENCE_ {
	int type;
	ULONG64 val;
} OBFSEQUENCE;

void asmJitIsBad(std::vector<uint8_t> bytes, x86::Assembler* trash) {
	for (uint8_t u : bytes) {
		trash->embedUInt8(u);
	}
}

//(2x+1)(x+1)(x) < ULLMAX when x <= 2097151

OBFSEQUENCE* generateSeq(int len) {
	
	OBFSEQUENCE* seq = (OBFSEQUENCE*)calloc(len,sizeof(OBFSEQUENCE));
	int lastType = getRand64() % 5;
	for (long long i = 0; i < len; i++) {
		do {
			seq[i].type = getRand64() % 5;
		} while (seq[i].type == lastType);
		lastType = seq[i].type;
		if (seq[i].type == TYPE_ROL) {
			seq[i].val = getRand64();
		} else if (seq[i].type == TYPE_XOR) {
			seq[i].val = getRand64();
		} else if (seq[i].type == TYPE_ROR_MAGIC) {
			seq[i].val = rand() % 2;
		} else if (seq[i].type == TYPE_MANIP_SYSCALL) {
			seq[i].val = getRand64() % ULLONG_MAX + 1;
		} else if (seq[i].type == TYPE_OPAQUE) {
			//seq[i].val = getRand64() % ULLONG_MAX + 1;
		}
	}
	return seq;
}

constexpr long long SEQ_LEN = 10000;

void obfuscator::add_custom_entry(PIMAGE_SECTION_HEADER* new_section) {

	std::vector<Label> labels;

	if (pe->get_path().find(".exe") != std::string::npos) {
		ULONG64 entryAddress = pe->get_nt()->OptionalHeader.AddressOfEntryPoint;

		OBFSEQUENCE* sequence = generateSeq(SEQ_LEN);
		for (long long i = 0; i < SEQ_LEN; i++) {
			OBFSEQUENCE obf = sequence[i];
			if (obf.type == TYPE_ROL) {
				entryAddress = _rotr64(entryAddress, __popcnt64(obf.val));
			} else if (obf.type == TYPE_XOR) {
				entryAddress = entryAddress ^ obf.val;
			} else if (obf.type == TYPE_ROR_MAGIC) {
				entryAddress = _rotl64(entryAddress, pe->get_buffer()->at(obf.val));
			} else if (obf.type == TYPE_OPAQUE) {
				entryAddress -= 0x40;
			}
		}
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

		for (long long i = SEQ_LEN-1; i >= 0; i--) {
			OBFSEQUENCE obf = sequence[i];
			if (obf.type == TYPE_ROL) {
				Label l = assm.newLabel();
				assm.bind(l);
				labels.push_back(l);
				assm.mov(lookupmap.find(ZYDIS_REGISTER_RCX)->second, obf.val);
				assm.popcnt(lookupmap.find(ZYDIS_REGISTER_RCX)->second, lookupmap.find(ZYDIS_REGISTER_RCX)->second);
				assm.rol(lookupmap.find(ZYDIS_REGISTER_RBX)->second, lookupmap.find(ZYDIS_REGISTER_CL)->second);
			} else if (obf.type == TYPE_XOR) {
				Label l = assm.newLabel();
				assm.bind(l);
				labels.push_back(l);
				assm.mov(lookupmap.find(ZYDIS_REGISTER_RCX)->second, obf.val);
				assm.xor_(lookupmap.find(ZYDIS_REGISTER_RBX)->second, lookupmap.find(ZYDIS_REGISTER_RCX)->second);
			} else if (obf.type == TYPE_ROR_MAGIC) {
				Label l = assm.newLabel();
				assm.bind(l);
				labels.push_back(l);
				assm.mov(lookupmap.find(ZYDIS_REGISTER_CL)->second, x86::byte_ptr(lookupmap.find(ZYDIS_REGISTER_RAX)->second, obf.val));
				assm.ror(lookupmap.find(ZYDIS_REGISTER_RBX)->second, lookupmap.find(ZYDIS_REGISTER_CL)->second);
			} else if (obf.type == TYPE_MANIP_SYSCALL) {
				Label l = assm.newLabel();
				assm.bind(l);
				labels.push_back(l);
				assm.push(lookupmap.find(ZYDIS_REGISTER_RAX)->second);
				assm.mov(lookupmap.find(ZYDIS_REGISTER_RDX)->second, obf.val);
				assm.mov(lookupmap.find(ZYDIS_REGISTER_RAX)->second, getSyscallId());
				assm.syscall();
				assm.pop(lookupmap.find(ZYDIS_REGISTER_RAX)->second);
				assm.add(lookupmap.find(ZYDIS_REGISTER_RBX)->second, lookupmap.find(ZYDIS_REGISTER_RDX)->second);
			} else if (obf.type == TYPE_OPAQUE){
				Label l = assm.newLabel();
				assm.bind(l);
				labels.push_back(l);
				asmJitIsBad({0x48, 0x8d, 0x0d, 00, 00, 00, 00}, &assm); //lea rcx, [rip+0]
				assm.test(x86::cl, 1);
				assm.pushfq();
				assm.pop(x86::rcx);
				assm.and_(x86::rcx, 0x40);
				assm.add(x86::rbx, x86::rcx);

				asmJitIsBad({ 0x48, 0x8d, 0x0d, 00, 00, 00, 00 }, &assm); //lea rcx, [rip+0]
				assm.test(x86::cl, 1);
				assm.pushfq();
				assm.pop(x86::rcx);
				assm.and_(x86::rcx, 0x40);
				assm.add(x86::rbx, x86::rcx);

				if (i%10000==0) {
					labels.clear();
				}

				if (labels.size() >= 2) {
					assm.cmp(x86::rsp, 0);
					assm.jz(labels.at(getRand64() % (labels.size()-1)));
				}
			}
		}


		free(sequence);
		for (int i = sizeof(pushRs) / sizeof(ZydisRegister) - 1; i >= 0; i--) {
			assm.pop(lookupmap.find(pushRs[i])->second);
		}
		assm.add(lookupmap.find(ZYDIS_REGISTER_RAX)->second, lookupmap.find(ZYDIS_REGISTER_RBX)->second);
		assm.pop(lookupmap.find(ZYDIS_REGISTER_RCX)->second);
		assm.pop(lookupmap.find(ZYDIS_REGISTER_RBX)->second);
		assm.popf();
		assm.xchg(lookupmap.find(ZYDIS_REGISTER_RAX)->second, qword_ptr(lookupmap.find(ZYDIS_REGISTER_RSP)->second,0));
		assm.ret();
		void* fn = nullptr;
		auto err = rt.add(&fn, &code);
		std::vector<obfuscator::instruction_t> jitinstructions = this->instructions_from_jit((uint8_t*)fn, code.codeSize());
		uint64_t offset = (uint64_t)(*new_section) - (uint64_t)pe->get_buffer()->data();
		pe->get_buffer()->resize(pe->get_buffer()->capacity() + jitinstructions.size() * 0xf);
		*new_section = (PIMAGE_SECTION_HEADER)(offset + pe->get_buffer()->data());

		for (auto inst = jitinstructions.begin(); inst != jitinstructions.end(); ++inst) {
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