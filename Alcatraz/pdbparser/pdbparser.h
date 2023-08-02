#pragma once
#include "../pe/pe.h"

#include <string>

class pdbparser {
private:

	struct codeviewInfo_t
	{
		ULONG CvSignature;
		GUID Signature;
		ULONG Age;
		char PdbFileName[ANYSIZE_ARRAY];
	};

	uint8_t* module_base;

public:

	struct sym_func {

		int id;

		uint32_t offset;
		std::string name;
		uint32_t size;
		bool obfuscate = true;
		bool ctfflattening = true;
		bool movobf = true;
		bool addobf = true;
		bool leaobf = true;
		bool antidisassembly = true;
		bool popobf = true;
		bool retobf = true;
		bool xorobf = true;
		bool cmpobf = true;
		bool callobf = true;
		bool stackobf = true;
		bool incobf = true;
	};

	pdbparser(pe64* pe);
	
	~pdbparser();

	std::vector<sym_func>parse_functions();

};