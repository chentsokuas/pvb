$ !
$ ! IF DON'T HAVE A MAKETOOL, USE THIS
$ !
$ SET VERI
$ COMPILER := CXX
$ LINKER   := CXXLINK
$
$ 'COMPILER' PVSERVER.C
$ 'COMPILER' MASK1.C
$ 'COMPILER' MASK2.C
$ 'COMPILER' MASK3.C
$ 'COMPILER' MASK4.C
$ 'COMPILER' PERIODIC.C
$ 'COMPILER' MASKVTK.C
$ 'COMPILER' PVBIMAGE.C
$ 'COMPILER' UTIL.C
$ 'COMPILER' WTHREAD.C
$ 'COMPILER' GLENCODE.C
$
$ DELETE PVSERVER.OLB;*
$ LIB/CREATE PVSERVER.OLB
$ LIB/REPLACE PVSERVER.OLB MASK1.OBJ
$ LIB/REPLACE PVSERVER.OLB MASK2.OBJ
$ LIB/REPLACE PVSERVER.OLB MASK3.OBJ
$ LIB/REPLACE PVSERVER.OLB MASK4.OBJ
$ LIB/REPLACE PVSERVER.OLB PERIODIC.OBJ
$ LIB/REPLACE PVSERVER.OLB MASKVTK.OBJ
$ LIB/REPLACE PVSERVER.OLB PVBIMAGE.OBJ
$ LIB/REPLACE PVSERVER.OLB UTIL.OBJ
$ LIB/REPLACE PVSERVER.OLB WTHREAD.OBJ
$ LIB/REPLACE PVSERVER.OLB GLENCODE.OBJ
$
$ 'LINKER' PVSERVER.OBJ,PVSERVER.OLB/LIB
$
$ SET NOVERI