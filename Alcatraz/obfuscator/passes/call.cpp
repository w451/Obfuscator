#include "../obfuscator.h"

/*
jmp 22		eb 22
jmp 21		eb 21
garbage[20]	90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90
jmp -24		eb e8
call func	func offset
*/

uint8_t garbage[] = { 0xEB, 0xFE, 0xEB, 0xFE, 0xEB, 0xFE, 0xEB, 0xFE, 0xEB, 0xFE, 0xEB, 0xFE, 0xEB, 0xFE, 0xEB, 0xFE, 0xEB, 0xFE, 0xEB, 0xFE, };

bool obfuscator::obfuscate_call(std::vector<obfuscator::function_t>::iterator& function, std::vector<obfuscator::instruction_t>::iterator& instruction) {
	if (instruction->raw_bytes.at(0) == 0xE8) { //32 bit offset call
		instruction_t firstJmp{}; firstJmp.load(function->func_id, { 0xEB, 22 });
		instruction_t secondJmp{}; secondJmp.load(function->func_id, { 0xEB, 21 });
		instruction_t thirdJmp{}; thirdJmp.load(function->func_id, { 0xEB });

		firstJmp.isjmpcall = false;
		firstJmp.has_relative = false;
		secondJmp.isjmpcall = false;
		secondJmp.has_relative = false;
		thirdJmp.isjmpcall = false;
		thirdJmp.has_relative = false;

		instruction = function->instructions.insert(instruction, firstJmp);
		instruction++;
		instruction = function->instructions.insert(instruction, secondJmp);
		instruction++;

		uint8_t random_garbage[20];
		uint8_t* targetGarbage = garbage;
		if (rand()%2==0) {
			for (int i = 0; i < 20; i++) {
				random_garbage[i] = rand() % 0x100;
			}
			targetGarbage = random_garbage;
		}

		for (int i = 0; i < 20; i ++) {
			instruction_t garbo{}; garbo.load(function->func_id, { targetGarbage[i] });
			garbo.isjmpcall = false;
			garbo.has_relative = false;
			instruction = function->instructions.insert(instruction, garbo);
			instruction++;
		}
		instruction = function->instructions.insert(instruction, thirdJmp);
		instruction++;
	}
	return true;
}