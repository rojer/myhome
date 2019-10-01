#!/usr/bin/env python3

import json
import time
import argparse
import binascii
import datetime

from bluepy import btle


# "off" is out, on is in.
# 0x001d 0xb'3013050813' b'0\x13\x05\x08\x13'
#   time and date, mmhhddMMYY
# 0x001f 0xb'0000000000000000' b'\x00\x00\x00\x00\x00\x00\x00\x00'
#   x 7 of these - programs
# 0x002d 0xb'170408131704081300' b'\x17\x04\x08\x13\x17\x04\x08\x13\x00'
#   x 8 of these, 0x2d - 0x3b
# 0x003d 0xb'000808' b'\x00\x08\x08'
#   0: mode,
#      bit 0: 0 - auto, 1 - manual
#      bit 1: ???
#      bit 2: ???
#      bit 3: ???
#      bit 4: ??? window function active?
#      bit 5: ???
#      bit 6: ??? set to 1 on first power on (byte entire value 0x40)
#      bit 7: ???
#   1: state:
#      bits 0-2: 0 - idle, 1 - moving, 2 - ???, 3 - error, 4, 5 - ???, 6 - adap, 7 - inst, 8 - ???, 0xe - ??? (first power on, date entry)
#      bit 3: low battery
#      bits 4-7: ??? - 0xc when first powered on and date is not set
#   2: ???, values seen: 0, 8
# 0x003f 0xb'32 39 28 29 28 04 0a' b'29()(\x04\n'
#   0: ambient temp (x 0.5 C), 0 = n/a
#   1: target temp (x 0.5 C)
#   2: energy saving temp (x 0.5 C)
#   3: comfort temp (x 0.5 C)
#   4: temp offset (x 0.5 C), int8
#   5: window function sensitivity, 4 - high, 8 - mid, 0xc - low
#   6: window function time, minutes
# 0x0041 0xb'4f' b'O'
#   0: battery percentage, 0xff = n/a
# 0x0043 0xb'434f424c30313236' b'COBL0126'
#   firmware version
# 0x0045 0xb'1e00' b'\x1e\x00'
#   0: display off idle time, seconds
#   1: display on time remaining
# 0x0047 - lock characteristic, write pin to unlock. default pin is 000000, write 0x00000000 to unlock.
#
#
# adv data, updated every ~25 seconds
# advertised 128-bit service: 47e9ee00-47e9-11e4-8939-164230d1df67
# venor-specific data (0xff), 7 bytes:
#   0: ambient temp (x 0.5 C), 0/0xff = n/a (same as 0x003f byte 0)
#   1: set temp (x 0.5 C), 0/0xff = n/a (same as 0x003f byte 1)
#   2: battery percentage (same as 0x0041)
#   3: mode (same as 0x003d byte 0)
#   4: state (same as 0x003d byte 1)
#   5: ???, always seems to be 0xff. reserved?
#   6: ???
#   7: ???
# e.g.: 340f460003ff06fc
# all fields can be 0xff

class XavaxRadiator(btle.Peripheral):

    FLAG_MODE = 1 << 0
    FLAG_LOCK = 1 << 7

    STATE_IDLE = 0
    STATE_MOVING = 1
    STATE_ERROR = 3
    STATE_ADAP = 6
    STATE_INST = 7

    TEMP_SET_OFF = 4.0
    TEMP_DISP_OFF = 7.5
    TEMP_DISP_ON = 28.5
    TEMP_SET_ON = 32.0

    STATE_NAMES = {
        0: "idle", 1: "move", 3: "ERR!", 6: "adap", 7: "INST",
    }

    def __init__(self, addr):
        super().__init__(addr, addrType=btle.ADDR_TYPE_PUBLIC)

    def unlock(self):
        self.writeCharacteristic(0x0047, bytes([0, 0, 0, 0]))

    def set_mode(self, manual, lock):
        flags = 0
        if manual: flags |= self.FLAG_MODE
        if lock: flags |= self.FLAG_LOCK
        self.writeCharacteristic(0x003d, bytes([flags, 0, 0]))

    def set_target_temp(self, temp):
        v = list(self.readCharacteristic(0x003f))
        v[1] = int(temp / 0.5)
        self.writeCharacteristic(0x003f, bytes(v))

    def set_disp_idle(self, disp_idle):
        self.writeCharacteristic(0x0045, bytes([disp_idle, 0]))

    def get_status(self):
        v3d = self.readCharacteristic(0x003d)
        v3f = self.readCharacteristic(0x003f)
        v41 = self.readCharacteristic(0x0041)
        print("%02x %02x %02x" % (v3d[0], v3d[1], v3d[2]))
        return {
            "mode": v3d[0] & 1,
            "state": int(v3d[1]) & 0xf,
            "amb_temp": (v3f[0] * 0.5 if v3f[0] != 0 else None),
            "tgt_temp": (v3f[1] * 0.5),
            "esv_temp": (v3f[2] * 0.5),
            "com_temp": (v3f[3] * 0.5),
            "temp_off": (v3f[4] * 0.5),
            "batt_pct": (int(v41[0]) if v41[0] not in (0, 0xff) else None),
        }

    def set_time(self, new_dt):
        data = self.readCharacteristic(0x001d)
        curdt = datetime.datetime(2000 + int(data[4]), int(data[3]), int(data[2]), hour=int(data[1]), minute=int(data[0]))
        print("Current device time: %s" % curdt)
        new_dt = datetime.datetime(
            new_dt.year, new_dt.month, new_dt.day,
            hour=new_dt.hour,
            minute = new_dt.minute + 1 if new_dt.second >= 30 else new_dt.minute,
        )
        new_data = bytes([new_dt.minute, new_dt.hour, new_dt.day, new_dt.month, new_dt.year - 2000])
        if new_data != data:
            self.writeCharacteristic(0x001d, new_data, withResponse=True)
            print("New device time: %s" % new_dt)
        else:
            print("Device time is correct")

    def dump(self):
        print("reading...")
        for h in range(0x001d, 0x0047, 2):
            v = self.readCharacteristic(h)
            print("0x%04x 0x%s %s" % (h, binascii.hexlify(v), v))

    @staticmethod
    def tstr(t):
        if t <= XavaxRadiator.TEMP_DISP_OFF:
            return "off"
        if t >= XavaxRadiator.TEMP_DISP_ON:
            return "on"
        return "%.1f" % t

    @classmethod
    def get_state_name(cls, sn):
        if sn == 0xff: return "n/a"
        sn &= 0x7
        name = cls.STATE_NAMES.get(sn)
        return name if name is not None else "?(%#x)" % sn

    def set_manual_on(self, on):
        self.set_mode(True, True)
        tt = XavaxRadiator.TEMP_SET_ON if on else XavaxRadiator.TEMP_SET_OFF
        self.set_target_temp(tt)



class ScanDelegate(btle.DefaultDelegate):

    def __init__(self, device_map=None):
        super().__init__()
        self._device_map = device_map or {}
        self._actions = []

    def get_name(self, addr):
        e = self._device_map.get(addr)
        if not e: return ""
        return e.get("name", "")

    def _get_desired_state(self, addr):
        e = self._device_map.get(addr)
        if not e: return
        return e.get("want_on", None)


    def handleDiscovery(self, data, isNewDev, isNewData):
        if data.getValueText(6) != "47e9ee00-47e9-11e4-8939-164230d1df67":
            return
        status_data = data.getValueText(0xff)
        sd = binascii.unhexlify(status_data)
        t, tt, bpct, flags, state = sd[:5]
        ts = ("%2.1f" % (t * 0.5)) if t != 0 and t != 0xff else "n/a"
        if tt == 0 or tt == 0xff:
            tts = "n/a"
        elif tt * 0.5 == XavaxRadiator.TEMP_SET_OFF:
            tts = "OFF"
        elif tt * 0.5 == XavaxRadiator.TEMP_SET_ON:
            tts = "ON"
        else:
            tts = ("%2.1f" % (tt * 0.5))
        bpcts = ("%2d%%" % bpct) if bpct != 0 and bpct <= 100 else "n/a"
        if flags != 0xff:
            fs = ""
            fs += ("L" if flags & 0x80 == 0x80 else ".")
            fs += ("M" if flags & 1 == 1 else "A")
        else:
            fs = "n/a"
        sts = XavaxRadiator.get_state_name(state)
        act, acts = None, ""
        want_on = self._get_desired_state(data.addr)
        can_control = (sts == "idle" and t != 0 and t != 0xff and tt != 0 and tt != 0xff)
        if want_on is not None and can_control:
            is_on = t < tt
            if want_on != is_on or flags & 0x81 != 0x81 or tt * 0.5 not in (XavaxRadiator.TEMP_SET_OFF, XavaxRadiator.TEMP_SET_ON):
                act = (data.addr, want_on)
                acts = " want %s" % ("ON" if want_on else "OFF")
                if act not in self._actions:
                    self._actions.append(act)

        print("%s (%-20s), status %4s [T %s TT %4s batt %s flags %s(%#02x) state %s(%#02x)]%s" % (
            data.addr, self.get_name(data.addr), status_data,
            ts, tts, bpcts, fs, flags, sts, state, acts))

    def get_actions(self):
        return self._actions


def LoadDeviceMap(map_file):
    if not map_file:
        return {}
    with open(map_file) as f:
        return json.load(f)


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("-m", "--map_file", dest="map_file", type=str, default=None, help="Device map file")

    sp = parser.add_subparsers(dest="action")

    scan_parser = sp.add_parser("scan", help="Scan for devices")
    scan_parser.add_argument("-t", "--time", type=float, default=10.0, help="Scan for this long")
    scan_parser.add_argument("-c", "--control", action="store_true", default=False, help="Perform control actions to reconcile with desired state in the map")

    on_parser = sp.add_parser("on", help="Turn heater on")
    on_parser.add_argument("mac_address", action="store", help="MAC address of the device")

    off_parser = sp.add_parser("off", help="Turn heater off")
    off_parser.add_argument("mac_address", action="store", help="MAC address of the device")

    set_time_parser = sp.add_parser("set_time", help="Set the device time")
    set_time_parser.add_argument("mac_address", action="store", help="MAC address of the device")

    status_parser = sp.add_parser("status", help="Get heater status")
    status_parser.add_argument("-i", "--interval", action="store", type=int, default=0, metavar="SECONDS", help="Interval, seconds")
    status_parser.add_argument("mac_address", action="store", help="MAC address of the device")

    args = parser.parse_args()

    device_map = LoadDeviceMap(args.map_file)

    if args.action == "scan":
        sd = ScanDelegate(device_map=device_map)
        scanner = btle.Scanner().withDelegate(sd)
        devices = scanner.scan(args.time)
        if args.control and sd.get_actions():
            for addr, want_on in sd.get_actions():
                print("%s (%s) -> %s" % (addr, sd.get_name(addr), ("ON" if want_on else "OFF")))
                try: 
                    r = XavaxRadiator(addr)
                    r.unlock()
                    r.set_manual_on(want_on)
                    r.disconnect()
                except Exception as e:
                    print("Failed to control %s: %s" % (addr, e))
        return

    print("connecting...")

    r = XavaxRadiator(args.mac_address)

    print("unlocking...")

    r.unlock()

    try:
        if args.action == "on":
            t.set_manual_on(True)
        elif args.action == "off":
            t.set_manual_on(False)
        elif args.action == "status":
            while True:
                s = r.get_status()
                print("mode %d, state %d, T %.1f, TT %s, batt %d%%" % (
                    s["mode"], s["state"], s["amb_temp"] or -1, r.tstr(s["tgt_temp"]), s["batt_pct"] or -1))
                if args.interval <= 0:
                    return
                time.sleep(args.interval)
        elif args.action == "set_time":
            r.set_time(datetime.datetime.now())
        else:
            while False:
                h = 0x003d
                v = r.readCharacteristic(h)
                print("0x%04x 0x%s %s" % (h, binascii.hexlify(v), v))
                time.sleep(0.5)
        #r.dump()


    finally:
        print("disconnecting")
        r.disconnect()


if __name__ == "__main__":
    main()
