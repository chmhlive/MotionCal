#ifndef BLEHID_H_
#define BLEHID_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MB01_VID 0x08E2
#define MB01_PID 0x0101

int blehid_enumerate(void);
int blehid_device_count(void);
const char *blehid_device_name(int index);
int blehid_is_device_name(const char *name);
int blehid_is_open(void);
int blehid_open(const char *name);
int blehid_read_sensor(void);
int blehid_write_cmd(unsigned char cmd, const unsigned char *data, int len);
void blehid_close(void);

#ifdef __cplusplus
}
#endif

#endif
