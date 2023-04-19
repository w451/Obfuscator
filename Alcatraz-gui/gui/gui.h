#pragma once

typedef struct GlobalArgs_ {
	bool obf_entry_point = false;
	bool outline_funcs = true;
	int outline_max = 100;
	int outline_strlen = 3;
	bool anti_xref = true;
} GlobalArgs, * PGlobalArgs;

namespace gui {
	void render_interface();
}