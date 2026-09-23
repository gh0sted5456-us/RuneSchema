import ctypes
import json
import pathlib
import sys

CALL = ctypes.CFUNCTYPE
LOG = CALL(None, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_char_p)
REGISTER_CAPABILITY = CALL(ctypes.c_int32, ctypes.c_char_p, ctypes.c_char_p)
SERVICE = CALL(ctypes.c_int32, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p,
               ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32))
REGISTER_SERVICE = CALL(ctypes.c_int32, ctypes.c_char_p, ctypes.c_char_p, SERVICE, ctypes.c_void_p)
CALL_SERVICE = CALL(ctypes.c_int32, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
                    ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32))


class Descriptor(ctypes.Structure):
    _fields_ = [("size", ctypes.c_uint32), ("api", ctypes.c_uint32),
                ("id", ctypes.c_char_p), ("name", ctypes.c_char_p), ("version", ctypes.c_char_p)]


class Host(ctypes.Structure):
    _fields_ = [("size", ctypes.c_uint32), ("api", ctypes.c_uint32), ("log", LOG),
                ("register_capability", REGISTER_CAPABILITY),
                ("register_service", REGISTER_SERVICE), ("call_service", CALL_SERVICE)]


capabilities = set()
services = {}
messages = []


@LOG
def log(level, plugin, message):
    messages.append((level, plugin.decode(), message.decode()))


@REGISTER_CAPABILITY
def register_capability(plugin, capability):
    value = (plugin.decode(), capability.decode())
    if value in capabilities:
        return 3
    capabilities.add(value)
    return 0


@REGISTER_SERVICE
def register_service(plugin, name, callback, context):
    key = name.decode()
    if key in services:
        return 3
    services[key] = (plugin.decode(), callback, context)
    return 0


@CALL_SERVICE
def call_service(caller, name, request, response, capacity, required):
    service = services.get(name.decode())
    if not service:
        return 4
    return service[1](service[2], request, response, capacity, required)


def main():
    dll = ctypes.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    dll.RuneSchemaPlugin_Query.restype = ctypes.POINTER(Descriptor)
    dll.RuneSchemaPlugin_Initialize.argtypes = [ctypes.POINTER(Host), ctypes.POINTER(ctypes.c_void_p)]
    dll.RuneSchemaPlugin_Initialize.restype = ctypes.c_int32
    dll.RuneSchemaPlugin_Shutdown.argtypes = [ctypes.c_void_p]
    descriptor = dll.RuneSchemaPlugin_Query().contents
    assert (descriptor.api, descriptor.id, descriptor.version) == (1, b"RuneSchema.Helpy", b"0.7.0")
    host = Host(ctypes.sizeof(Host), 1, log, register_capability, register_service, call_service)
    instance = ctypes.c_void_p()
    assert dll.RuneSchemaPlugin_Initialize(ctypes.byref(host), ctypes.byref(instance)) == 0
    assert len(capabilities) == 6 and "helpy.about" in services
    needed = ctypes.c_uint32()
    assert call_service(b"verification", b"helpy.about", b"{}", None, 0, ctypes.byref(needed)) == 5
    response = ctypes.create_string_buffer(needed.value)
    assert call_service(b"verification", b"helpy.about", b"{}", response, len(response), ctypes.byref(needed)) == 0
    about = json.loads(response.value)
    assert about == {"plugin": "RuneSchema.Helpy", "version": "0.7.0", "ui": "plugin-dll", "umg": False}
    dll.RuneSchemaPlugin_Shutdown(instance)
    print(f"PASS: {descriptor.id.decode()} lifecycle, {len(capabilities)} capabilities, helpy.about={about}")


if __name__ == "__main__":
    main()
