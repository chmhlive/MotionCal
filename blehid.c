#include "blehid.h"

#include "debuglog.h"
#include "imuread.h"
#include "hidapi/hidapi.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define BLEHID_MAX_DEVICES 8
#define BLEHID_READ_TIMEOUT_MS 1
#define BLEHID_READBACK_TIMEOUT_ATTEMPTS 25
#define BLEHID_READBACK_TIMEOUT_MS 20

typedef struct {
	char name[128];
	char path[512];
} BleHidDeviceInfo;

static BleHidDeviceInfo devices[BLEHID_MAX_DEVICES];
static int device_count;
static hid_device *handle;
static int read_count;

static int clamp_int16(float value)
{
	if (value > 32767.0f) return 32767;
	if (value < -32768.0f) return -32768;
	return (int)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

static float read_le_float(const unsigned char *p)
{
	unsigned char tmp[4];
	float value;

	tmp[0] = p[0];
	tmp[1] = p[1];
	tmp[2] = p[2];
	tmp[3] = p[3];
	memcpy(&value, tmp, sizeof(value));
	return value;
}

static unsigned short read_le_u16(const unsigned char *p)
{
	return (unsigned short)(p[0] | (p[1] << 8));
}

static void write_le_u16(unsigned char *p, unsigned short value)
{
	p[0] = (unsigned char)value;
	p[1] = (unsigned char)(value >> 8);
}

static void wide_to_utf8(const wchar_t *src, char *dst, size_t dst_len)
{
	if (dst_len == 0) return;
	dst[0] = 0;
	if (src == NULL) return;
#if defined(WINDOWS)
	WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, (int)dst_len, NULL, NULL);
	dst[dst_len - 1] = 0;
#else
	wcstombs(dst, src, dst_len - 1);
	dst[dst_len - 1] = 0;
#endif
}

static const char *hid_error_text(void)
{
	static char text[512];
	wide_to_utf8(hid_error(handle), text, sizeof(text));
	return text[0] ? text : "unknown";
}

static int find_device_index(const char *name)
{
	int i;

	for (i = 0; i < device_count; i++) {
		if (strcmp(devices[i].name, name) == 0) return i;
	}
	return -1;
}

static int send_run_mode(unsigned char mode)
{
	unsigned char data[1];
	data[0] = mode;
	return blehid_write_cmd(1, data, 1);
}

static void write_le_float(unsigned char *p, float value)
{
	memcpy(p, &value, sizeof(value));
}

static int is_float_ok(float actual, float expected)
{
	float err = fabsf(actual - expected);
	float maxerr = 0.0001f + fabsf(expected) * 0.00003f;
	return err <= maxerr;
}

static void parse_report3_payload(const unsigned char *data, int len, unsigned char report_id)
{
	float accel[3], gyro[3], mag[3];
	int16_t raw[9];
	int i;

	if (len < 40) return;
	for (i = 0; i < 3; i++) accel[i] = read_le_float(data + 4 + i * 4);
	for (i = 0; i < 3; i++) gyro[i] = read_le_float(data + 16 + i * 4);
	for (i = 0; i < 3; i++) mag[i] = read_le_float(data + 28 + i * 4);

	for (i = 0; i < 3; i++) raw[i] = (int16_t)clamp_int16(accel[i] / 9.80665f / G_PER_COUNT);
	for (i = 0; i < 3; i++) raw[3 + i] = (int16_t)clamp_int16(gyro[i] / DEG_PER_SEC_PER_COUNT);
	for (i = 0; i < 3; i++) raw[6 + i] = (int16_t)clamp_int16(mag[i] / UT_PER_COUNT);

	read_count++;
	if (read_count <= 5 || debuglog_is_verbose()) {
		debuglog_printf("hid report id=%u acc=%.3f,%.3f,%.3f gyro=%.3f,%.3f,%.3f mag=%.3f,%.3f,%.3f",
			(unsigned)report_id, accel[0], accel[1], accel[2], gyro[0], gyro[1], gyro[2], mag[0], mag[1], mag[2]);
	}
	raw_data(raw);
}

int blehid_enumerate(void)
{
	struct hid_device_info *list;
	struct hid_device_info *dev;
	int index = 0;

	device_count = 0;
	if (hid_init() != 0) {
		debuglog_printf("hid_init failed");
		return 0;
	}
	list = hid_enumerate(MB01_VID, MB01_PID);
	for (dev = list; dev != NULL && index < BLEHID_MAX_DEVICES; dev = dev->next) {
		char product[64];
		char serial[64];

		wide_to_utf8(dev->product_string, product, sizeof(product));
		wide_to_utf8(dev->serial_number, serial, sizeof(serial));
		snprintf(devices[index].name, sizeof(devices[index].name), "MB01 HID %d%s%s%s",
			index + 1,
			product[0] ? " - " : "",
			product[0] ? product : "",
			serial[0] ? "" : "");
		if (serial[0]) {
			size_t used = strlen(devices[index].name);
			snprintf(devices[index].name + used, sizeof(devices[index].name) - used, " [%s]", serial);
		}
		strncpy(devices[index].path, dev->path, sizeof(devices[index].path) - 1);
		devices[index].path[sizeof(devices[index].path) - 1] = 0;
		debuglog_printf("hid enumerate found name=%s path=%s", devices[index].name, devices[index].path);
		index++;
	}
	hid_free_enumeration(list);
	device_count = index;
	debuglog_printf("hid enumerate VID=%04X PID=%04X found=%d", MB01_VID, MB01_PID, device_count);
	return device_count;
}

int blehid_device_count(void)
{
	return device_count;
}

const char *blehid_device_name(int index)
{
	if (index < 0 || index >= device_count) return NULL;
	return devices[index].name;
}

int blehid_is_device_name(const char *name)
{
	if (name == NULL) return 0;
	if (find_device_index(name) >= 0) return 1;
	blehid_enumerate();
	return find_device_index(name) >= 0;
}

int blehid_is_open(void)
{
	return handle != NULL;
}

int blehid_open(const char *name)
{
	int index;

	if (handle != NULL) blehid_close();
	index = find_device_index(name);
	if (index < 0) {
		blehid_enumerate();
		index = find_device_index(name);
	}
	if (index < 0) {
		debuglog_printf("hid open failed: device name not found name=%s", name ? name : "(null)");
		return 0;
	}
	handle = hid_open_path(devices[index].path);
	if (handle == NULL) {
		debuglog_printf("hid open failed name=%s path=%s error=%s", devices[index].name, devices[index].path, hid_error_text());
		return 0;
	}
	read_count = 0;
	debuglog_printf("hid open ok name=%s path=%s", devices[index].name, devices[index].path);
	if (blehid_set_mag_cal_enabled(0) < 0) {
		debuglog_printf("hid CMD8 disable mag calibration failed error=%s", hid_error_text());
		blehid_close();
		return 0;
	}
	if (!blehid_confirm_mag_cal_enabled(0)) {
		debuglog_printf("hid mag calibration disable readback failed");
		blehid_close();
		return 0;
	}
	if (send_run_mode(1) < 0) {
		debuglog_printf("hid CMD1 start failed error=%s", hid_error_text());
		blehid_close();
		return 0;
	}
	debuglog_printf("hid CMD1 mode=1 sent");
	return 1;
}

int blehid_read_sensor(void)
{
	unsigned char buf[64];
	int n;

	if (handle == NULL) return -1;
	n = hid_read_timeout(handle, buf, sizeof(buf), BLEHID_READ_TIMEOUT_MS);
	if (n < 0) {
		debuglog_printf("hid read failed error=%s", hid_error_text());
		blehid_close();
		return -1;
	}
	if (n == 0) return 0;
	debuglog_verbose_printf("hid read bytes=%d report=%u", n, (unsigned)buf[0]);
	if (buf[0] == 3 && n >= 41) {
		parse_report3_payload(buf + 1, n - 1, buf[0]);
	} else if (buf[0] == 6 && n >= 43) {
		parse_report3_payload(buf + 1, 40, buf[0]);
	} else {
		debuglog_verbose_printf("hid ignored report id=%u bytes=%d", (unsigned)buf[0], n);
	}
	return n;
}

int blehid_write_cmd(unsigned char cmd, const unsigned char *data, int len)
{
	unsigned char report[27];
	int n;

	if (handle == NULL) return -1;
	if (len < 0) len = 0;
	if (len > 25) len = 25;
	memset(report, 0, sizeof(report));
	report[0] = 1;
	report[1] = cmd;
	if (data != NULL && len > 0) memcpy(report + 2, data, (size_t)len);
	n = hid_write(handle, report, sizeof(report));
	debuglog_verbose_printf("hid write cmd=%u len=%d result=%d", (unsigned)cmd, len, n);
	return n;
}

static int blehid_read_param(unsigned short offset, unsigned char len, unsigned char *out)
{
	unsigned char req[3];
	unsigned char buf[64];
	int attempt;

	if (handle == NULL || out == NULL || len == 0 || len > 25) return 0;
	write_le_u16(req, offset);
	req[2] = len;
	if (blehid_write_cmd(12, req, sizeof(req)) < 0) {
		debuglog_printf("hid CMD12 request failed offset=0x%04X len=%u error=%s",
			(unsigned)offset, (unsigned)len, hid_error_text());
		return 0;
	}
	debuglog_printf("hid CMD12 read offset=0x%04X len=%u", (unsigned)offset, (unsigned)len);
	for (attempt = 0; attempt < BLEHID_READBACK_TIMEOUT_ATTEMPTS; attempt++) {
		int n = hid_read_timeout(handle, buf, sizeof(buf), BLEHID_READBACK_TIMEOUT_MS);
		if (n < 0) {
			debuglog_printf("hid CMD12 readback failed offset=0x%04X error=%s",
				(unsigned)offset, hid_error_text());
			return 0;
		}
		if (n == 0) continue;
		if (buf[0] == 3 && n >= 41) {
			parse_report3_payload(buf + 1, n - 1, buf[0]);
			continue;
		}
		if (buf[0] == 6 && n >= 43) {
			parse_report3_payload(buf + 1, 40, buf[0]);
			continue;
		}
		if (buf[0] == 4 && n >= 4) {
			unsigned short got_offset = read_le_u16(buf + 1);
			unsigned char got_len = buf[3];
			if (got_offset != offset) {
				debuglog_printf("hid CMD12 ignored offset=0x%04X while waiting for 0x%04X",
					(unsigned)got_offset, (unsigned)offset);
				continue;
			}
			if (got_len < len || n < (int)(4 + len)) {
				debuglog_printf("hid CMD12 short response offset=0x%04X got_len=%u bytes=%d expected=%u",
					(unsigned)offset, (unsigned)got_len, n, (unsigned)len);
				return 0;
			}
			memcpy(out, buf + 4, len);
			debuglog_printf("hid CMD12 readback offset=0x%04X len=%u ok", (unsigned)offset, (unsigned)len);
			return 1;
		}
		debuglog_verbose_printf("hid CMD12 ignored report id=%u bytes=%d", (unsigned)buf[0], n);
	}
	debuglog_printf("hid CMD12 readback timeout offset=0x%04X len=%u", (unsigned)offset, (unsigned)len);
	return 0;
}

int blehid_write_mag_cal(unsigned char index, float value)
{
	unsigned char data[5];
	int n;

	data[0] = index;
	write_le_float(data + 1, value);
	n = blehid_write_cmd(8, data, sizeof(data));
	debuglog_printf("hid CMD8 idx=%u value=%.6f result=%d", (unsigned)index, value, n);
	return n;
}

int blehid_set_mag_cal_enabled(int enabled)
{
	int n = blehid_write_mag_cal(0, enabled ? 1.0f : 0.0f);
	if (n >= 0) debuglog_printf("hid CMD8 mag_cal_enable=%d sent", enabled ? 1 : 0);
	return n;
}

int blehid_confirm_mag_cal_enabled(int enabled)
{
	unsigned char value;

	if (!blehid_read_param(0x002E, 1, &value)) return 0;
	if (value != (unsigned char)(enabled ? 1 : 0)) {
		debuglog_printf("hid mag_cal_enable mismatch expected=%d actual=%u",
			enabled ? 1 : 0, (unsigned)value);
		return 0;
	}
	debuglog_printf("hid mag_cal_enable confirmed=%d", enabled ? 1 : 0);
	return 1;
}

int blehid_confirm_mag_cal_values(const float hard_iron[3], const float soft_iron[3][3])
{
	unsigned char hard[12];
	unsigned char soft0[24];
	unsigned char soft1[12];
	float actual;
	int i, row, col;

	if (!blehid_read_param(0x0030, sizeof(hard), hard)) return 0;
	if (!blehid_read_param(0x003C, sizeof(soft0), soft0)) return 0;
	if (!blehid_read_param(0x0054, sizeof(soft1), soft1)) return 0;
	for (i = 0; i < 3; i++) {
		actual = read_le_float(hard + i * 4);
		if (!is_float_ok(actual, hard_iron[i])) {
			debuglog_printf("hid cal hard[%d] mismatch expected=%.6f actual=%.6f", i, hard_iron[i], actual);
			return 0;
		}
		debuglog_printf("hid cal hard[%d] expected=%.6f actual=%.6f ok", i, hard_iron[i], actual);
	}
	for (row = 0; row < 3; row++) {
		for (col = 0; col < 3; col++) {
			int index = row * 3 + col;
			const unsigned char *p = index < 6 ? soft0 + index * 4 : soft1 + (index - 6) * 4;
			actual = read_le_float(p);
			if (!is_float_ok(actual, soft_iron[row][col])) {
				debuglog_printf("hid cal soft[%d][%d] mismatch expected=%.6f actual=%.6f",
					row, col, soft_iron[row][col], actual);
				return 0;
			}
			debuglog_printf("hid cal soft[%d][%d] expected=%.6f actual=%.6f ok",
				row, col, soft_iron[row][col], actual);
		}
	}
	debuglog_printf("hid mag calibration readback confirmed");
	return 1;
}

void blehid_close(void)
{
	if (handle == NULL) return;
	send_run_mode(0);
	debuglog_printf("hid CMD1 mode=0 sent");
	if (blehid_set_mag_cal_enabled(1) >= 0) {
		blehid_confirm_mag_cal_enabled(1);
	}
	hid_close(handle);
	handle = NULL;
	debuglog_printf("hid close");
}
