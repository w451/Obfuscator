#include "../obfuscator.h"


int currentFuncId = LONG_MAX - 0x100000;
int currentInstrId = LONG_MAX - 0x100000;
#define NEXT_FUNC() (currentFuncId++)
#define NEXT_INSTR() (currentInstrId++)

int instrStrLen = 2;
bool jmpLinking = true;

bool isInstrValid(obfuscator::instruction_t inst1) {



	return inst1.zyinstr.mnemonic != ZYDIS_MNEMONIC_NOP && inst1.zyinstr.mnemonic != ZYDIS_MNEMONIC_INVALID && inst1.zyinstr.mnemonic != ZYDIS_MNEMONIC_RET && !is_jmpcall(inst1.zyinstr) && !is_stackop(inst1.zyinstr) && !is_ripop(inst1.zyinstr);
}

bool isInstrsValids(obfuscator::function_t func, int ind1, int ind2) {
	for (int i = ind1; i < ind2; i++) {
		if (!isInstrValid(func.instructions.at(i))) {
			return false;
		}
	}
	return true;
}

bool cmpInstrs(obfuscator::instruction_t inst1, obfuscator::instruction_t inst2) {
	if (inst1.raw_bytes.size() != inst2.raw_bytes.size()) {
		return false;
	}
	for (int i = 0; i < inst1.raw_bytes.size(); i++) {
		if (inst1.raw_bytes.at(i) != inst2.raw_bytes.at(i)) {
			return false;
		}
	}
	return true;
}

bool isUnique(std::vector<std::pair<std::vector<obfuscator::instruction_t>, int>>* insts, obfuscator::function_t func, LONG64 ind) {
	for (int m = 0; m < insts->size(); m++) {
		std::pair<std::vector<obfuscator::instruction_t>, int> ab = insts->at(m);
		int j = 0;
		for (int i = 0; i < instrStrLen; i++) {
			if (cmpInstrs(ab.first.at(i), func.instructions.at(ind+i))) {
				j++;
			}
		}
		if (j == instrStrLen) {
			insts->at(m).second++;
			return false;
		}
	}
	return true;
}

bool hasJmpsTo(obfuscator::function_t* func, int index, int endindex) {
	for (obfuscator::instruction_t fi : func->instructions) {
		if (fi.isjmpcall) {
			for (int i = index; i <= endindex; i++) {
				if (fi.relative.target_inst_id == func->instructions.at(i).inst_id) {
					return true;
				}
			}
		}
	}
	return false;
}

bool obfuscator::outline_instrs(std::vector<obfuscator::function_t>* functions, int maxOutline, int ilen, bool xrefer) {
	instrStrLen = ilen;
	jmpLinking = xrefer;

	std::vector<std::pair<std::vector<obfuscator::instruction_t>, int>> instrstrs;

	for (LONG64 i = 0; i < functions->size(); i++) {
		printf("Outlining function %d/%d\n",i+1, functions->size());
		if (instrstrs.size() >= maxOutline) {
			break;
		}
		obfuscator::function_t lefunc = functions->at(i);
		if (lefunc.instructions.size() < instrStrLen) {
			continue;
		}
		for (LONG64 j = 0; j < lefunc.instructions.size() - instrStrLen + 1; j++) {
			if (!isInstrsValids(lefunc,j,j + instrStrLen)) {
				continue;
			}

			bool unique = isUnique(&instrstrs, lefunc, j);
			
			if (unique) {
				std::pair<std::vector<obfuscator::instruction_t>, int> newinststr;
				newinststr.first = std::vector<obfuscator::instruction_t>();
				for (int m = 0; m < instrStrLen; m++) {
					newinststr.first.push_back(lefunc.instructions.at(j + m));
				}
				newinststr.second = 0;
				instrstrs.push_back(newinststr);
			}
		}
	}



	for (int p = 0; p < instrstrs.size(); p++) {
		if (instrstrs.at(p).second == 0) {
			instrstrs.erase(instrstrs.begin() + p);
			p--;
		}
	}

	for (int p = 0; p < instrstrs.size(); p++) {
		//printf("lel %d\n", instrstrs.at(p).second);
		for (int l = 0; l < instrstrs.at(p).first.size(); l++) {
			//instrstrs.at(p).first.at(l).print();

		}
	}

	int inserteds = 0;

	for (std::pair<std::vector<obfuscator::instruction_t>, int> inststr : instrstrs) {
		std::vector<obfuscator::instruction_t> proxyJmps;

		obfuscator::function_t newFunc(NEXT_FUNC(), "lol", -1, 0);
		int firstInst = 0;
		for (int i = 0; i < instrStrLen; i++) {
			obfuscator::instruction_t newinstr = { 0 };
			std::vector<uint8_t> que;
			for (uint8_t u8 : inststr.first.at(i).raw_bytes) {
				que.push_back(u8);
			}
			newinstr.load(newFunc.func_id, que);
			newinstr.inst_id = NEXT_INSTR();
			newinstr.has_relative = false;
			newinstr.isjmpcall = false;
			if (firstInst == 0) {
				firstInst = newinstr.inst_id;
			}
			newFunc.instructions.push_back(newinstr);
			newFunc.size += newinstr.raw_bytes.size();
		}
		obfuscator::instruction_t ret_instr = {0};
		ret_instr.load(newFunc.func_id, {0xc3});
		ret_instr.inst_id = NEXT_INSTR();
		ret_instr.has_relative = false;
		ret_instr.isjmpcall = false;
		newFunc.instructions.push_back(ret_instr);

		bool insertFunc = false;
		ULONG64 ss = functions->size();
		for (LONG64 fa = 0; fa < ss; fa++) {
			

			for (LONG64 ins = 0; ins < (LONG64)(functions->at(fa).instructions.size()) - instrStrLen + 1; ins++) {

				bool hit = true;

				for (LONG64 k = 0; k < instrStrLen; k++) {
					if (!cmpInstrs(functions->at(fa).instructions.at(ins + k), inststr.first.at(k))) {
						hit = false;
						break;
					}
				}
			
				if (hit) {
					
					if (hasJmpsTo(&functions->at(fa), ins+1, ins + instrStrLen)) {
						continue;
					}

					insertFunc = true;
					int firstId = functions->at(fa).instructions.at(ins).inst_id;
					for (LONG64 k = 0; k < instrStrLen; k++) {
						functions->at(fa).instructions.erase(functions->at(fa).instructions.begin()+ins);
					}

					//printf("inserted call id %d targetting %d\n", firstId, firstInst);
					if (jmpLinking) {
						obfuscator::instruction_t push_rax = { 0 };
						push_rax.load(functions->at(fa).func_id, { 0x50 });
						push_rax.has_relative = false;
						push_rax.isjmpcall = false;
						push_rax.inst_id = firstId;

						obfuscator::instruction_t pushf = { 0 };
						pushf.load(functions->at(fa).func_id, { 0x66, 0x9c });
						pushf.has_relative = false;
						pushf.isjmpcall = false;
						pushf.inst_id = NEXT_INSTR();

						obfuscator::instruction_t lea_rip = { 0 };
						lea_rip.load(functions->at(fa).func_id, { 0x48, 0x8d, 0x05, 0, 0, 0, 0 });
						lea_rip.has_relative = false;
						lea_rip.isjmpcall = false;
						lea_rip.inst_id = NEXT_INSTR();

						obfuscator::instruction_t add_rax = { 0 };
						add_rax.load(functions->at(fa).func_id, { 0x48, 0x83, 0xc0, 0x10 });
						add_rax.has_relative = false;
						add_rax.isjmpcall = false;
						add_rax.inst_id = NEXT_INSTR();

						obfuscator::instruction_t xchg_rax = {0}; 
						xchg_rax.load(functions->at(fa).func_id, { 0x48, 0x87, 0x44, 0x24 ,0x02 });
						xchg_rax.has_relative = false;
						xchg_rax.isjmpcall = false;
						xchg_rax.inst_id = NEXT_INSTR();

						obfuscator::instruction_t popf = { 0 };
						popf.load(functions->at(fa).func_id, { 0x66, 0x9d });
						popf.has_relative = false;
						popf.isjmpcall = false;
						popf.inst_id = NEXT_INSTR();

						obfuscator::instruction_t jmp_instr = { 0 };
						jmp_instr.load(functions->at(fa).func_id, { 0xe9, 0, 0, 0, 0 });
						jmp_instr.inst_id = NEXT_INSTR();
						jmp_instr.has_relative = true;
						jmp_instr.isjmpcall = true;
						jmp_instr.relative.size = 32;

						if (proxyJmps.size() == 0) {
							jmp_instr.relative.target_func_id = newFunc.func_id;
							jmp_instr.relative.target_inst_id = firstInst;
						} else {
							obfuscator::instruction_t target_jmp = proxyJmps.at(rand() % proxyJmps.size());
							jmp_instr.relative.target_func_id = target_jmp.func_id;
							jmp_instr.relative.target_inst_id = target_jmp.inst_id;
						}

						int a = 0;
						functions->at(fa).instructions.insert(functions->at(fa).instructions.begin() + ins + a++, push_rax);
						functions->at(fa).instructions.insert(functions->at(fa).instructions.begin() + ins + a++, pushf);
						functions->at(fa).instructions.insert(functions->at(fa).instructions.begin() + ins + a++, lea_rip);
						functions->at(fa).instructions.insert(functions->at(fa).instructions.begin() + ins + a++, add_rax);
						functions->at(fa).instructions.insert(functions->at(fa).instructions.begin() + ins + a++, xchg_rax);
						functions->at(fa).instructions.insert(functions->at(fa).instructions.begin() + ins + a++, popf);
						functions->at(fa).instructions.insert(functions->at(fa).instructions.begin() + ins + a++, jmp_instr);
							
						
						proxyJmps.push_back(jmp_instr);
						
					} else {
						obfuscator::instruction_t call_instr = { 0 };
						call_instr.load(functions->at(fa).func_id, { 0xe8, 0, 0, 0, 0 });
						call_instr.inst_id = firstId;
						call_instr.has_relative = true;
						call_instr.isjmpcall = true;
						call_instr.relative.target_func_id = newFunc.func_id;
						call_instr.relative.target_inst_id = firstInst;
						call_instr.relative.size = 32;
						functions->at(fa).instructions.insert(functions->at(fa).instructions.begin() + ins, call_instr);
					}
				}
			}
		}
		if (insertFunc) {
			functions->push_back(newFunc);
			inserteds++;
		}
	}
	printf("Inserted %d outlined functions\n", inserteds);
}