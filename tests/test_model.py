import ctypes as C
import subprocess
from pathlib import Path
import pytest
ROOT=Path(__file__).resolve().parents[1]
class Snapshot(C.Structure):
    _fields_=[('regs',C.c_uint16*60),('coils',C.c_uint16),('received_ms',C.c_uint64),('sequence',C.c_uint32),('online',C.c_bool)]
class Command(C.Structure):
    _fields_=[('fc',C.c_uint8),('address',C.c_uint16),('value',C.c_int32),('stopped_only',C.c_bool),('id',C.c_uint32)]
@pytest.fixture(scope='session')
def lib(tmp_path_factory):
    so=tmp_path_factory.mktemp('core')/'model.so'
    subprocess.run(['cc','-shared','-fPIC','-std=c11','-Wall','-Wextra','-Wno-misleading-indentation','-Werror','-I',str(ROOT/'include'),str(ROOT/'src/domain/model.c'),'-o',str(so)],check=True)
    l=C.CDLL(str(so));l.mex_crc.argtypes=[C.c_void_p,C.c_size_t];l.mex_crc.restype=C.c_uint16
    l.mex_request.argtypes=[C.c_uint8,C.c_uint8,C.c_uint16,C.c_uint16,C.c_void_p];l.mex_request.restype=C.c_size_t
    l.mex_response.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t]
    l.mex_validate.argtypes=[C.POINTER(Command),C.c_bool,C.c_bool];l.mex_validate.restype=C.c_bool
    l.mex_running.argtypes=[C.POINTER(Snapshot)];l.mex_running.restype=C.c_bool
    l.mex_stages.argtypes=[C.POINTER(Snapshot),C.c_uint];l.mex_stages.restype=C.c_uint8
    l.mex_alarms.argtypes=[C.POINTER(Snapshot)];l.mex_alarms.restype=C.c_uint32
    l.mex_schedule.argtypes=[C.c_int64,C.c_int64,C.c_int,C.c_int,C.c_void_p,C.c_void_p]
    return l
def packet(l,b):return b+l.mex_crc(b,len(b)).to_bytes(2,'little')
def test_document_coil_frame(lib):
    out=C.create_string_buffer(8);lib.mex_request(1,1,0,8,out)
    assert out.raw==bytes.fromhex('01 01 00 00 00 08 3d cc')
def test_response_identity_crc_length(lib):
    q=bytes.fromhex('01 03 00 23 00 02 00 00');v=(C.c_uint16*2)()
    p=packet(lib,bytes.fromhex('01 03 04 ff e7 01 f4'))
    assert lib.mex_response(q,p,len(p),v,2)==0
    assert list(v)==[65511,500]
    assert lib.mex_response(q,p[:-1],len(p)-1,v,2)<0
    assert lib.mex_response(q,p,len(p),v,1)<0
    bad=packet(lib,bytes.fromhex('02 03 04 ff e7 01 f4'));assert lib.mex_response(q,bad,len(bad),v,2)<0
    bad=bytearray(p);bad[4]^=1;assert lib.mex_response(q,bytes(bad),len(bad),v,2)<0
    exception=packet(lib,bytes.fromhex('01 83 02'));assert lib.mex_response(q,exception,len(exception),v,2)==2
@pytest.mark.parametrize('a,lo,hi',[(40012,50,500),(40013,200,800),(40014,1,100),(40016,10,100),(40021,-100,100),(40022,-300,300),(40027,249,500),(40059,1000,20000)])
def test_writable_ranges(lib,a,lo,hi):
    for val in [lo,hi]:assert lib.mex_validate(C.byref(Command(6,a,val,False,0)),True,False)
    for val in [lo-1,hi+1]:assert not lib.mex_validate(C.byref(Command(6,a,val,False,0)),True,False)
def test_read_only_and_interlock(lib):
    for a in [40002,40006,40020,40023,40036]:assert not lib.mex_validate(C.byref(Command(6,a,1,False,0)),True,False)
    assert not lib.mex_validate(C.byref(Command(5,10,1,False,0)),True,True)
    assert lib.mex_validate(C.byref(Command(5,10,1,False,0)),True,False)
    assert not lib.mex_validate(C.byref(Command(6,40012,250,False,0)),False,False)
    assert not lib.mex_validate(C.byref(Command(6,40021,0,False,0)),True,True)
def test_delay_byte_is_not_running(lib):
    s=Snapshot();s.regs[5]=0x0500;assert not lib.mex_running(C.byref(s));s.regs[5]|=1;assert lib.mex_running(C.byref(s))
def test_independent_stage_and_named_alarms(lib):
    s=Snapshot();s.regs[6]=0x82;s.regs[7]=0x10;s.regs[8]=2
    assert lib.mex_stages(C.byref(s),1)==2 # only unit 2, never fill unit 1
    assert lib.mex_stages(C.byref(s),4)==8
    assert lib.mex_stages(C.byref(s),2)==16
    s.regs[3]=1025;assert lib.mex_alarms(C.byref(s))==1 # documented live fan + panel LED
    s.regs[4]=(1<<11)|(1<<15)|(1<<4);assert lib.mex_alarms(C.byref(s))==1|(1<<11)|(1<<15)
@pytest.mark.parametrize('c,s,d,rev,p',[(250,230,40,0,50),(200,230,30,1,100),(180,230,30,0,0),(300,230,0,0,0),(500,230,10,0,100)])
def test_proportional_formula(lib,c,s,d,rev,p):assert lib.mex_proportion(c,s,d,rev)==p
def test_schedule_no_replay(lib):
    on=(C.c_uint16*7)(*([540]*7));off=(C.c_uint16*7)(*([1080]*7))
    assert lib.mex_schedule(99,100,0,540,on,off)==1
    assert lib.mex_schedule(100,100,0,540,on,off)==-1
    assert lib.mex_schedule(-1,100,0,540,on,off)==-1
    assert lib.mex_schedule(98,100,0,540,on,off)==-1
    assert lib.mex_schedule(101,100,0,540,on,off)==-1
    on[0]=off[0];assert lib.mex_schedule(99,100,0,1080,on,off)==-1
