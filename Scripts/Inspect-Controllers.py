import ctypes as c, json
from ctypes import wintypes as w
u=c.WinDLL('user32'); h=c.WinDLL('hid')
class Device(c.Structure): _fields_=[('handle',w.HANDLE),('kind',w.DWORD)]
u.GetRawInputDeviceList.argtypes=[c.POINTER(Device),c.POINTER(w.UINT),w.UINT]
u.GetRawInputDeviceInfoW.argtypes=[w.HANDLE,w.UINT,w.LPVOID,c.POINTER(w.UINT)]
h.HidP_GetCaps.argtypes=[w.LPVOID,w.LPVOID]
h.HidP_GetValueCaps.argtypes=[c.c_int,w.LPVOID,c.POINTER(c.c_ushort),w.LPVOID]
n=w.UINT();u.GetRawInputDeviceList(None,c.byref(n),c.sizeof(Device));devices=(Device*n.value)();u.GetRawInputDeviceList(devices,c.byref(n),c.sizeof(Device))
for d in devices:
    size=w.UINT();u.GetRawInputDeviceInfoW(d.handle,0x20000007,None,c.byref(size));name=c.create_unicode_buffer(size.value+1);u.GetRawInputDeviceInfoW(d.handle,0x20000007,name,c.byref(size))
    if 'VID_054C' not in name.value.upper(): continue
    size=w.UINT();u.GetRawInputDeviceInfoW(d.handle,0x20000005,None,c.byref(size));data=c.create_string_buffer(size.value);u.GetRawInputDeviceInfoW(d.handle,0x20000005,data,c.byref(size))
    caps=c.create_string_buffer(64);h.HidP_GetCaps(data,caps); count=c.c_ushort.from_buffer(caps,48);values=c.create_string_buffer(count.value*72);h.HidP_GetValueCaps(0,values,c.byref(count),data)
    print(name.value)
    for i in range(count.value):
        off=i*72
        print('Axis',i+1,'UsagePage',c.c_ushort.from_buffer(values,off).value,'Usage',hex(c.c_ushort.from_buffer(values,off+56).value),'min',c.c_int32.from_buffer(values,off+40).value,'max',c.c_int32.from_buffer(values,off+44).value)
