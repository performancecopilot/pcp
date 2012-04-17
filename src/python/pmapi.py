import ctypes, os, sys
from ctypes import *

try:
    PMAPI_VERSION_2 = 2
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 41
try:
    PMAPI_VERSION = PMAPI_VERSION_2
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 47
try:
    PM_ID_NULL = 4294967295
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 50
try:
    PM_INDOM_NULL = 4294967295
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 51
try:
    PM_IN_NULL = 4294967295
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 53
try:
    PM_NS_DEFAULT = NULL
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 86
try:
    PM_SPACE_BYTE = 0
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 87
try:
    PM_SPACE_KBYTE = 1
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 88
try:
    PM_SPACE_MBYTE = 2
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 89
try:
    PM_SPACE_GBYTE = 3
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 90
try:
    PM_SPACE_TBYTE = 4
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 91
try:
    PM_SPACE_PBYTE = 5
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 92
try:
    PM_SPACE_EBYTE = 6
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 94
try:
    PM_TIME_NSEC = 0
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 95
try:
    PM_TIME_USEC = 1
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 96
try:
    PM_TIME_MSEC = 2
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 97
try:
    PM_TIME_SEC = 3
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 98
try:
    PM_TIME_MIN = 4
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 99
try:
    PM_TIME_HOUR = 5
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 105
try:
    PM_COUNT_ONE = 0
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 117
try:
    PM_TYPE_NOSUPPORT = (-1)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 118
try:
    PM_TYPE_32 = 0
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 119
try:
    PM_TYPE_U32 = 1
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 120
try:
    PM_TYPE_64 = 2
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 121
try:
    PM_TYPE_U64 = 3
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 122
try:
    PM_TYPE_FLOAT = 4
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 123
try:
    PM_TYPE_DOUBLE = 5
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 124
try:
    PM_TYPE_STRING = 6
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 125
try:
    PM_TYPE_AGGREGATE = 7
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 126
try:
    PM_TYPE_AGGREGATE_STATIC = 8
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 127
try:
    PM_TYPE_EVENT = 9
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 128
try:
    PM_TYPE_UNKNOWN = 255
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 131
try:
    PM_SEM_COUNTER = 1
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 133
try:
    PM_SEM_INSTANT = 3
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 134
try:
    PM_SEM_DISCRETE = 4
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 136
try:
    PM_ERR_BASE2 = 12345
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 137
try:
    PM_ERR_BASE = PM_ERR_BASE2
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 141
try:
    PM_ERR_GENERIC = ((-PM_ERR_BASE) - 0)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 142
try:
    PM_ERR_PMNS = ((-PM_ERR_BASE) - 1)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 143
try:
    PM_ERR_NOPMNS = ((-PM_ERR_BASE) - 2)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 144
try:
    PM_ERR_DUPPMNS = ((-PM_ERR_BASE) - 3)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 145
try:
    PM_ERR_TEXT = ((-PM_ERR_BASE) - 4)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 146
try:
    PM_ERR_APPVERSION = ((-PM_ERR_BASE) - 5)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 147
try:
    PM_ERR_VALUE = ((-PM_ERR_BASE) - 6)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 149
try:
    PM_ERR_TIMEOUT = ((-PM_ERR_BASE) - 8)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 150
try:
    PM_ERR_NODATA = ((-PM_ERR_BASE) - 9)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 151
try:
    PM_ERR_RESET = ((-PM_ERR_BASE) - 10)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 153
try:
    PM_ERR_NAME = ((-PM_ERR_BASE) - 12)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 154
try:
    PM_ERR_PMID = ((-PM_ERR_BASE) - 13)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 155
try:
    PM_ERR_INDOM = ((-PM_ERR_BASE) - 14)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 156
try:
    PM_ERR_INST = ((-PM_ERR_BASE) - 15)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 157
try:
    PM_ERR_UNIT = ((-PM_ERR_BASE) - 16)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 158
try:
    PM_ERR_CONV = ((-PM_ERR_BASE) - 17)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 159
try:
    PM_ERR_TRUNC = ((-PM_ERR_BASE) - 18)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 160
try:
    PM_ERR_SIGN = ((-PM_ERR_BASE) - 19)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 161
try:
    PM_ERR_PROFILE = ((-PM_ERR_BASE) - 20)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 162
try:
    PM_ERR_IPC = ((-PM_ERR_BASE) - 21)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 164
try:
    PM_ERR_EOF = ((-PM_ERR_BASE) - 23)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 165
try:
    PM_ERR_NOTHOST = ((-PM_ERR_BASE) - 24)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 166
try:
    PM_ERR_EOL = ((-PM_ERR_BASE) - 25)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 167
try:
    PM_ERR_MODE = ((-PM_ERR_BASE) - 26)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 168
try:
    PM_ERR_LABEL = ((-PM_ERR_BASE) - 27)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 169
try:
    PM_ERR_LOGREC = ((-PM_ERR_BASE) - 28)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 170
try:
    PM_ERR_NOTARCHIVE = ((-PM_ERR_BASE) - 29)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 171
try:
    PM_ERR_LOGFILE = ((-PM_ERR_BASE) - 30)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 172
try:
    PM_ERR_NOCONTEXT = ((-PM_ERR_BASE) - 31)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 173
try:
    PM_ERR_PROFILESPEC = ((-PM_ERR_BASE) - 32)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 174
try:
    PM_ERR_PMID_LOG = ((-PM_ERR_BASE) - 33)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 175
try:
    PM_ERR_INDOM_LOG = ((-PM_ERR_BASE) - 34)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 176
try:
    PM_ERR_INST_LOG = ((-PM_ERR_BASE) - 35)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 177
try:
    PM_ERR_NOPROFILE = ((-PM_ERR_BASE) - 36)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 178
try:
    PM_ERR_NOAGENT = ((-PM_ERR_BASE) - 41)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 179
try:
    PM_ERR_PERMISSION = ((-PM_ERR_BASE) - 42)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 180
try:
    PM_ERR_CONNLIMIT = ((-PM_ERR_BASE) - 43)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 181
try:
    PM_ERR_AGAIN = ((-PM_ERR_BASE) - 44)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 182
try:
    PM_ERR_ISCONN = ((-PM_ERR_BASE) - 45)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 183
try:
    PM_ERR_NOTCONN = ((-PM_ERR_BASE) - 46)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 184
try:
    PM_ERR_NEEDPORT = ((-PM_ERR_BASE) - 47)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 186
try:
    PM_ERR_NONLEAF = ((-PM_ERR_BASE) - 49)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 189
try:
    PM_ERR_TYPE = ((-PM_ERR_BASE) - 52)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 190
try:
    PM_ERR_THREAD = ((-PM_ERR_BASE) - 53)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 193
try:
    PM_ERR_TOOSMALL = ((-PM_ERR_BASE) - 98)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 194
try:
    PM_ERR_TOOBIG = ((-PM_ERR_BASE) - 99)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 195
try:
    PM_ERR_FAULT = ((-PM_ERR_BASE) - 100)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 197
try:
    PM_ERR_PMDAREADY = ((-PM_ERR_BASE) - 1048)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 198
try:
    PM_ERR_PMDANOTREADY = ((-PM_ERR_BASE) - 1049)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 199
try:
    PM_ERR_NYI = ((-PM_ERR_BASE) - 8999)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 207
try:
    PM_MAXERRMSGLEN = 128
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 220
try:
    PMNS_LOCAL = 1
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 221
try:
    PMNS_REMOTE = 2
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 222
try:
    PMNS_ARCHIVE = 3
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 241
try:
    PMNS_LEAF_STATUS = 0
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 242
try:
    PMNS_NONLEAF_STATUS = 1
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 311
try:
    PM_CONTEXT_UNDEF = (-1)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 312
try:
    PM_CONTEXT_HOST = 1
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 313
try:
    PM_CONTEXT_ARCHIVE = 2
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 314
try:
    PM_CONTEXT_LOCAL = 3
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 389
try:
    PM_VAL_HDR_SIZE = 4
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 390
try:
    PM_VAL_VLEN_MAX = 16777215
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 408
try:
    PM_VAL_INSITU = 0
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 409
try:
    PM_VAL_DPTR = 1
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 410
try:
    PM_VAL_SPTR = 2
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 447
try:
    PMCD_NO_CHANGE = 0
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 448
try:
    PMCD_ADD_AGENT = 1
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 449
try:
    PMCD_RESTART_AGENT = 2
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 450
try:
    PMCD_DROP_AGENT = 4
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 475
try:
    PM_TZ_MAXLEN = 40
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 476
try:
    PM_LOG_MAXHOSTLEN = 64
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 477
try:
    PM_LOG_MAGIC = 1342514688
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 478
try:
    PM_LOG_VERS02 = 2
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 479
try:
    PM_LOG_VOL_TI = (-2)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 480
try:
    PM_LOG_VOL_META = (-1)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 514
try:
    PM_MODE_LIVE = 0
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 515
try:
    PM_MODE_INTERP = 1
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 516
try:
    PM_MODE_FORW = 2
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 517
try:
    PM_MODE_BACK = 3
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 525
try:
    PM_TEXT_ONELINE = 1
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 526
try:
    PM_TEXT_HELP = 2
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 547
try:
    PM_XTB_FLAG = 16777216
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 549
def PM_XTB_SET(type):
    return (PM_XTB_FLAG | (type << 16))

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 550
def PM_XTB_GET(x):
    return (x & PM_XTB_FLAG) and ((x & 16711680) >> 16) or (-1)

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 631
try:
    PM_EVENT_FLAG_POINT = (1 << 0)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 632
try:
    PM_EVENT_FLAG_START = (1 << 1)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 633
try:
    PM_EVENT_FLAG_END = (1 << 2)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 634
try:
    PM_EVENT_FLAG_ID = (1 << 3)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 635
try:
    PM_EVENT_FLAG_PARENT = (1 << 4)
except:
    pass

# /work/scox/pcp/src/src/include/pcp/pmapi.h: 636
try:
    PM_EVENT_FLAG_MISSED = (1 << 31)
except:
    pass

# No inserted files
