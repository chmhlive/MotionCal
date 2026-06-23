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
	(void)ptr;
	debuglog_printf("hid calibration write not implemented len=%d", len);
	return -1;
}

void close_device(void)
{
	blehid_close();
}
