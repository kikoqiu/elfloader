This simple app try to load x86 elf executables on the M$ Windows platform. It uses cygwin1.dll for glibc replacement. 
Through it's just a toy now, it's able to run some small apps. Try to run a.out which is compiled from t.cc with centos 6 x86 gcc.
Author kikoqiu kikoqiu[at]163.com

Q&A
1, How to build for cygwin?
cd elfloader
make
2, How to test?
./elfloader.exe a.out