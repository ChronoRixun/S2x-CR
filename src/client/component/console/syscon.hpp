#pragma once

namespace syscon
{
	void init();

	void set_title(const std::string& title);

	void Sys_Print(const char* msg);

	// Console thread only: appends every queued line to the window.
	void Sys_FlushPending();
}