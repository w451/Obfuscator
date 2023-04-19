#include "../obfuscator.h"
#include<vector>

/*typedef struct Factors_ {
	ULONG64 count;
	LONG64 factors[];
} Factors, * PFactors;

typedef struct Polynomial_ {
	ULONG64 count;
	LONG64 coeffs[];
} Polynomial, * PPolynomial;

void generateFactors(ULONG64 degree, LONG64 maxVal, PFactors facs, LONG64 oneSolution, OPTIONAL PFactors exclusions = 0) {
	facs->count = degree;
	for (LONG64 i = 0; i < degree; i++) {
		bool isExclusive = false;
		do {
			facs->factors[i] = (rand() % (maxVal * 2 + 1)) - maxVal; //[-maxR,maxR]
			isExclusive = true;
			if (exclusions != 0) {
				for (LONG64 j = 0; j < degree; j++) {
					if (exclusions->factors[j] == facs->factors[i]) {
						isExclusive = false;
						break;
					}
				}
			}
		} while (!isExclusive);
	}
	facs->factors[rand() % degree] = -oneSolution;
}

void makePolynomial(PFactors factors, PPolynomial polynomial) {
	for (LONG64 m = 1; m <= factors->count; m++) {
		LONG64 n = factors->count;
		LONG64 k = m;
		std::vector<LONG64> indices(k, 0);
		for (int i = 0; i < k; i++) {
			indices[i] = i;
		}
		while (true) {
			LONG64 val = 1;
			for (LONG64 i = 0; i < k; i++) {
				val *= factors->factors[indices[i]];
			}
			polynomial->coeffs[k] += val;
			LONG64 i = k - 1;
			while (i >= 0 && indices[i] == i + n - k) {
				i--;
			}
			if (i < 0) break;
			indices[i]++;
			for (LONG64 j = i + 1; j < k; j++) {
				indices[j] = indices[j - 1] + 1;
			}
		}
	}
}

const LONG64 polynomialDegree = 7;
const LONG64 polynomialMax = 35;

//x > a   =   x / a > 0 {a > 0}
bool obfuscator::obfuscate_cmp(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {

	if (instruction->zyinstr.operands[0].reg.value != ZYDIS_REGISTER_NONE
		&& instruction->zyinstr.operands[0].reg.value != ZYDIS_REGISTER_R15D
		&& instruction->zyinstr.operands[0].reg.value != ZYDIS_REGISTER_R14D
		&& instruction->zyinstr.operands[0].reg.value != ZYDIS_REGISTER_R15
		&& instruction->zyinstr.operands[0].reg.value != ZYDIS_REGISTER_R14
		&& instruction->zyinstr.operands[0].reg.value != ZYDIS_REGISTER_RSP
		&& instruction->zyinstr.operands[0].size == 32
		&& instruction->zyinstr.operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
		if (instruction->test_cmp && (instruction->zyinstr.operands[1].imm.value.s <= polynomialMax && instruction->zyinstr.operands[1].imm.value.s >= -polynomialMax)) {
			PFactors firstFactors = (PFactors)calloc(1, sizeof(Factors) + sizeof(LONG64) * polynomialDegree);
			PFactors secondFactors = (PFactors)calloc(1, sizeof(Factors) + sizeof(LONG64) * polynomialDegree);

			generateFactors(polynomialDegree, polynomialMax, firstFactors, instruction->zyinstr.operands[1].imm.value.s);
			generateFactors(polynomialDegree, polynomialMax, secondFactors, instruction->zyinstr.operands[1].imm.value.s, firstFactors);

			PPolynomial firstPoly = (PPolynomial)calloc(1, sizeof(Factors) + sizeof(LONG64) * (polynomialDegree + 1));
			PPolynomial secondPoly = (PPolynomial)calloc(1, sizeof(Factors) + sizeof(LONG64) * (polynomialDegree + 1));
			makePolynomial(firstFactors, firstPoly);
			makePolynomial(secondFactors, secondPoly);
			free(firstFactors);
			free(secondFactors);

			assm.push(lookupmap.find(ZYDIS_REGISTER_R14)->second);
			assm.push(lookupmap.find(ZYDIS_REGISTER_R15)->second);
			assm.push(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second);
			assm.pushf();
			assm.xor_(lookupmap.find(ZYDIS_REGISTER_R14)->second, lookupmap.find(ZYDIS_REGISTER_R14)->second);
			assm.xor_(lookupmap.find(ZYDIS_REGISTER_R15)->second, lookupmap.find(ZYDIS_REGISTER_R15)->second);

			ZydisRegister first;
			ZydisRegister second;

			if (instruction->zyinstr.operands[0].size == 64) {
				first = ZYDIS_REGISTER_R15;
				second = ZYDIS_REGISTER_R14;
			} else if (instruction->zyinstr.operands[0].size == 32) {
				first = ZYDIS_REGISTER_R15D;
				second = ZYDIS_REGISTER_R14D;
			}

			for (int i = 0; i <= polynomialDegree; i++) {
				if (firstPoly->coeffs[i] != 0 && firstPoly->coeffs[i] != secondPoly->coeffs[i]) {
					assm.mov(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, 1);
					for (int j = polynomialDegree - i; j > 0; j--) {
						assm.imul(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, ptr(lookupmap.find(ZYDIS_REGISTER_RSP)->second, 0));
					}
					assm.imul(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, firstPoly->coeffs[i]);
					assm.add(lookupmap.find(first)->second, lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second);
				}
			}

			for (int i = 0; i <= polynomialDegree; i++) {
				if (secondPoly->coeffs[i] != 0 && firstPoly->coeffs[i] != secondPoly->coeffs[i]) {
					assm.mov(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, 1);
					for (int j = polynomialDegree - i; j > 0; j--) {
						assm.imul(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, ptr(lookupmap.find(ZYDIS_REGISTER_RSP)->second, 0));
					}
					assm.imul(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second, secondPoly->coeffs[i]);
					assm.add(lookupmap.find(second)->second, lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second);
				}
			}
			assm.popf();
			assm.cmp(lookupmap.find(second)->second, lookupmap.find(first)->second);

			assm.pop(lookupmap.find(instruction->zyinstr.operands[0].reg.value)->second);
			assm.pop(lookupmap.find(ZYDIS_REGISTER_R15)->second);
			assm.pop(lookupmap.find(ZYDIS_REGISTER_R14)->second);


			free(firstPoly);
			free(secondPoly);

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
}*/

bool obfuscator::obfuscate_cmp(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {
	return true;
}