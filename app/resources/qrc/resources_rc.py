# -*- coding: utf-8 -*-

# Resource object code
#
# Created by: The Resource Compiler for PyQt5 (Qt v5.15.10)
#
# WARNING! All changes made in this file will be lost!

from PyQt5 import QtCore

qt_resource_data = b"\
\x00\x00\x00\x12\
\x7b\
\x20\x22\x65\x6e\x74\x72\x69\x65\x73\x22\x3a\x20\x5b\x5d\x20\x7d\
\x0a\
"

qt_resource_name = b"\
\x00\x13\
\x08\xf3\xe4\x7e\
\x00\x6f\
\x00\x70\x00\x65\x00\x6e\x00\x67\x00\x6c\x00\x5f\x00\x62\x00\x75\x00\x67\x00\x6c\x00\x69\x00\x73\x00\x74\x00\x2e\x00\x6a\x00\x73\
\x00\x6f\x00\x6e\
"

qt_resource_struct_v1 = b"\
\x00\x00\x00\x00\x00\x02\x00\x00\x00\x01\x00\x00\x00\x01\
\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\
"

qt_resource_struct_v2 = b"\
\x00\x00\x00\x00\x00\x02\x00\x00\x00\x01\x00\x00\x00\x01\
\x00\x00\x00\x00\x00\x00\x00\x00\
\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\
\x00\x00\x01\x8a\x65\xf0\x72\x3f\
"

qt_version = [int(v) for v in QtCore.qVersion().split('.')]
if qt_version < [5, 8, 0]:
    rcc_version = 1
    qt_resource_struct = qt_resource_struct_v1
else:
    rcc_version = 2
    qt_resource_struct = qt_resource_struct_v2


def qInitResources():
    QtCore.qRegisterResourceData(rcc_version, qt_resource_struct, qt_resource_name, qt_resource_data)


def qCleanupResources():
    QtCore.qUnregisterResourceData(rcc_version, qt_resource_struct, qt_resource_name, qt_resource_data)


qInitResources()
