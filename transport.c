#include "imuread.h"
#include "debuglog.h"
#include "blehid.h"

int device_is_open(void)
{
	return blehid_is_open();
}

int open_device(const char *name)
{
	return blehid_open(name);
}

int read_device_data(void)
{
	return blehid_read_sensor();
}

int write_device_data(const void *ptr, int len)
{
	if (ptr == NULL || len < 1) return -1;
	return blehid_write_cmd(((const unsigned char *)ptr)[0], (const unsigned char *)ptr + 1, len - 1);
}

void close_device(void)
{
	blehid_close();
}
