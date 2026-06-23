#include "gui.h"
#include "blehid.h"

wxArrayString device_list()
{
	wxArrayString list;

	blehid_enumerate();
	for (int i = 0; i < blehid_device_count(); i++) {
		const char *name = blehid_device_name(i);
		if (name != NULL) list.Add(wxString::FromUTF8(name));
	}
	list.Sort();
	return list;
}
