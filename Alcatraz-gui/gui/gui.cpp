#include "gui.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#include "../interface/interface.h"

#include <d3d11.h>
#include <tchar.h>
#include <filesystem>
#include "imgui/imgui_internal.h"



std::string path = "";

int panel = 0;
int selected_func = 0;
char func_name[1024];
GlobalArgs globalArgs;
std::vector<pdbparser::sym_func>funcs;
std::vector<pdbparser::sym_func>funcs_to_obfuscate;
std::vector<std::string>logs;


void gui::render_interface() {
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(0, 0);
	
	ImGui::SetNextWindowSize(ImVec2(1280, 800));
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::Begin("Alcaztaz",0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollWithMouse);

	if (ImGui::BeginMenuBar()) {

		if (ImGui::BeginMenu("File")) {

			if (ImGui::MenuItem("Open")) {

				char filename[MAX_PATH];

				OPENFILENAMEA ofn;
				ZeroMemory(&filename, sizeof(filename));
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = NULL; 
				ofn.lpstrFilter = "Executables\0*.exe\0Dynamic Link Libraries\0*.dll\0Drivers\0*.sys";
				ofn.lpstrFile = filename;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrTitle = "Select your file.";
				ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;
				GetOpenFileNameA(&ofn);

				if (!std::filesystem::exists(filename)) {
					MessageBoxA(0, "Couldn't find file!", "Error", 0);
				}
				else {
					path = filename;
					funcs_to_obfuscate.clear();
					funcs.clear();
					try {
						funcs_to_obfuscate = inter::load_context(path);
					}
					catch (std::runtime_error e)
					{
						MessageBoxA(0, e.what(), "Exception", 0);
						path = "";
					}
					selected_func = 0;					
				}

			}

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	if (path.size()) {

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.f));
		ImGui::BeginChild("selectionpanel", ImVec2(100, 800), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		if (ImGui::Button("Protection", ImVec2(100, 100)))
			panel = 0;

		ImGui::EndChild();


		if (panel == 0) {
			ImGui::SetNextWindowPos(ImVec2(100, 25));

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.24f, 0.24f, 0.24f, 1.f));
			if (ImGui::BeginChild("optionpanel", ImVec2(300, 775), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {

				
				ImGui::SetNextItemWidth(300);
				ImGui::InputTextWithHint("", "Search", func_name, 1024);

				int showingTargets = funcs_to_obfuscate.size();
				for (auto func : funcs_to_obfuscate) {
					if (func.name.find(func_name) == std::string::npos) {
						showingTargets--;
					}
				}
				std::string targetStr;
				if (showingTargets == funcs_to_obfuscate.size()) {
					targetStr = (std::string("Target funcs (") + std::to_string(funcs_to_obfuscate.size()) + ")");
				} else {
					targetStr = (std::string("Target funcs (")+std::to_string(showingTargets) +"/" + std::to_string(funcs_to_obfuscate.size()) + ")");
				}
				if (ImGui::TreeNodeHashless(targetStr.c_str())) {
					for (int i = 0; i < funcs_to_obfuscate.size(); i++)
					{
						if (funcs_to_obfuscate.at(i).name.find(func_name) != std::string::npos && ImGui::Button(funcs_to_obfuscate.at(i).name.c_str()))
							selected_func = funcs_to_obfuscate.at(i).id;

					}
					ImGui::TreePop();
				}



				int showingExempted = funcs.size();
				for (auto func : funcs) {
					if (func.name.find(func_name) == std::string::npos) {
						showingExempted--;
					}
				}
				std::string exStr;
				if (showingExempted == funcs.size()) {
					exStr = (std::string("Exempt funcs (") + std::to_string(funcs.size()) + ")");
				} else {
					exStr = (std::string("Exempt funcs (") + std::to_string(showingExempted) + "/" + std::to_string(funcs.size()) + ")");
				}
				
				if (ImGui::TreeNodeHashless(exStr.c_str())) {
					for (int i = 0; i < funcs.size(); i++)
					{
						if (funcs.at(i).name.find(func_name) != std::string::npos && ImGui::Button(funcs.at(i).name.c_str()))
							selected_func = funcs.at(i).id;

					}
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Globals")) {
					ImGui::BeginChild("insidie2", ImVec2(300, globalArgs.outline_funcs ? 185 : 50)); //Need a child due to a bug in imgui causing the treenode to count as grabbing the slider...
					ImGui::Checkbox("Obfuscate entry point", &globalArgs.obf_entry_point);
					ImGui::Checkbox("Outline functions", &globalArgs.outline_funcs);
					if (globalArgs.outline_funcs) {
						ImGui::Text("Max outlined funcs");
						ImGui::SliderInt("", &globalArgs.outline_max, 1, 5000);
						ImGui::BeginChild("insidie3", ImVec2(300, 50));
						ImGui::Text("Outline instr len");
						ImGui::SliderInt("", &globalArgs.outline_strlen, 2, 5);
						ImGui::EndChild();
						ImGui::Checkbox("Anti Xref", &globalArgs.anti_xref);
					}
					ImGui::EndChild();
					ImGui::TreePop();
				}

				ImGui::BeginChild("insidie", ImVec2(300, 125), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

				if (ImGui::Button("Target all")) {
					for (auto func = funcs.begin(); func != funcs.end(); ++func) {
						funcs_to_obfuscate.push_back(*func);
						func = funcs.erase(func);
						func--;
					}
				}

				if (ImGui::Button("Exempt all")) {
					for (auto func = funcs_to_obfuscate.begin(); func != funcs_to_obfuscate.end(); ++func) {
						funcs.push_back(*func);
						func = funcs_to_obfuscate.erase(func);
						func--;
					}
				}

				if (ImGui::Button("Compile")) {
					try {
						inter::run_obfuscator(funcs_to_obfuscate, globalArgs);
					}
					catch (std::runtime_error e)
					{
						MessageBoxA(0, e.what(), "Exception", 0);
						path = "";
						//std::cout << "Runtime error: " << e.what() << std::endl;
					}

					MessageBoxA(0, "Compiled", "Success", 0);
				}

				
				ImGui::EndChild();
				ImGui::EndChild();
			}
			
			ImGui::SetNextWindowPos(ImVec2(400, 25));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.48f, 0.48f, 0.48f, 0.94f));
			ImGui::BeginChild("functionpanel", ImVec2(880,775), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);


			
			auto obfFunc = std::find_if(funcs_to_obfuscate.begin(), funcs_to_obfuscate.end(), [&](const pdbparser::sym_func infunc) { return infunc.id == selected_func; });
			auto exmFunc = std::find_if(funcs.begin(), funcs.end(), [&](const pdbparser::sym_func infunc) { return infunc.id == selected_func; });

			bool inObf = obfFunc != funcs_to_obfuscate.end();
			bool inExempt = exmFunc != funcs.end();

			auto func = inObf ? obfFunc : exmFunc;

			ImGui::Text("Name : %s", func->name.c_str());
			ImGui::Text("Address : %x", func->offset);
			ImGui::Text("Size : %i bytes", func->size);

			ImGui::Checkbox("Control flow flattening", &func->ctfflattening);
			ImGui::Checkbox("MOV obfuscation", &func->movobf);
			ImGui::Checkbox("Add obfuscation", &func->addobf);
			ImGui::Checkbox("LEA obfuscation", &func->leaobf);
			ImGui::Checkbox("Anti disassembly *", &func->antidisassembly);
			ImGui::Checkbox("Pop obfuscation", &func->popobf);
			ImGui::Checkbox("Ret obfuscation", &func->retobf);
			ImGui::Checkbox("Xor obfuscation", &func->xorobf);
			ImGui::Checkbox("Cmp obfuscation", &func->cmpobf);
			ImGui::Checkbox("Call obfuscation *", &func->callobf);
			ImGui::Checkbox("Stack obfuscation", &func->stackobf);

			if (ImGui::Button("Apply current settings to all")) {
				pdbparser::sym_func current;

				for (int i = 0; i < funcs_to_obfuscate.size(); i++) {
					if (funcs_to_obfuscate.at(i).id == selected_func) {
						current = funcs_to_obfuscate.at(i);
					}
				}
				for (int i = 0; i < funcs.size(); i++) {
					if (funcs.at(i).id == selected_func) {
						current = funcs.at(i);
					}
				}
				for (int i = 0; i < funcs.size(); i++) {
					if (selected_func != funcs.at(i).id) {
						funcs.at(i).ctfflattening = current.ctfflattening;
						funcs.at(i).movobf = current.movobf;
						funcs.at(i).addobf = current.addobf;
						funcs.at(i).leaobf = current.leaobf;
						funcs.at(i).antidisassembly = current.antidisassembly;
						funcs.at(i).popobf = current.popobf;
						funcs.at(i).retobf = current.retobf;
						funcs.at(i).xorobf = current.xorobf;
						funcs.at(i).cmpobf = current.cmpobf;
						funcs.at(i).callobf = current.callobf;
						funcs.at(i).stackobf = current.stackobf;
					}
				}
				for (int i = 0; i < funcs_to_obfuscate.size(); i++) {
					if (selected_func != funcs_to_obfuscate.at(i).id) {
						funcs_to_obfuscate.at(i).ctfflattening = current.ctfflattening;
						funcs_to_obfuscate.at(i).movobf = current.movobf;
						funcs_to_obfuscate.at(i).addobf = current.addobf;
						funcs_to_obfuscate.at(i).leaobf = current.leaobf;
						funcs_to_obfuscate.at(i).antidisassembly = current.antidisassembly;
						funcs_to_obfuscate.at(i).popobf = current.popobf;
						funcs_to_obfuscate.at(i).retobf = current.retobf;
						funcs_to_obfuscate.at(i).xorobf = current.xorobf;
						funcs_to_obfuscate.at(i).cmpobf = current.cmpobf;
						funcs_to_obfuscate.at(i).callobf = current.callobf;
						funcs_to_obfuscate.at(i).stackobf = current.stackobf;
					}
				}
			}
			if (inObf) {
				if (ImGui::Button("Exempt")) {
					funcs.push_back(*func);
					funcs_to_obfuscate.erase(func);
				}
			}
			if (inExempt) {
				if (ImGui::Button("Target")) {
					funcs_to_obfuscate.push_back(*func);
					funcs.erase(func);
				}
			}

				
				
			
			ImGui::SetCursorPosY(700);
			ImGui::Text("* Feature generates overlapping instructions which may not be compatible with other obfuscators/virtualizers");
			ImGui::Text(path.c_str());

			ImGui::EndChild();

			ImGui::PopStyleColor();

			ImGui::PopStyleColor();

		}
		

		ImGui::PopStyleColor();

		
	}

	ImGui::End();

}
