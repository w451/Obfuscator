#include "../obfuscator.h"

__forceinline int _strcmp(const char* s1, const char* s2)
{
	while (*s1 && (*s1 == *s2))
	{
		s1++;
		s2++;
	}
	return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

__declspec(safebuffers) int obfuscator::custom_dll_main(HINSTANCE instance, DWORD fdwreason, LPVOID reserved) {

	return 0;
}
void obfuscator::custom_dll_main_end() {};


__declspec(safebuffers) int obfuscator::custom_main(int argc, char* argv[]) {

	auto peb = (uint64_t)__readgsqword(0x60);
	auto base = *(uint64_t*)(peb + 0x10);
	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + ((PIMAGE_DOS_HEADER)base)->e_lfanew);

	PIMAGE_SECTION_HEADER section = nullptr;
	auto first = IMAGE_FIRST_SECTION(nt);
	for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
		auto currsec = first[i];

		char dev[5] = { '.', '0', 'D', 'e', 'v' };
		if (!_strcmp((char*)currsec.Name, dev)) {
			section = &currsec;
		}
	}

	uint32_t real_entry = *(uint32_t*)(base + section->VirtualAddress);
	real_entry ^= nt->OptionalHeader.SizeOfStackCommit;
	real_entry = _rotr(real_entry, nt->FileHeader.TimeDateStamp);
	return reinterpret_cast<int(*)(int, char**)>(base + real_entry)(argc, argv);
}
void obfuscator::custom_main_end() {};