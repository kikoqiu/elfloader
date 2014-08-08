// elfloader.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include<iomanip>  
#include "elfcpp.h"
#include "elfcpp_file.h"
#include <assert.h>
#include "i386.h"
#include <map>
using namespace std;


#include "map_file.h"
#include "util.h"
#include "exec_buffer.h"

bool verbose=false;

typedef elfcpp::Elf_file<32,false,File> ElfFile;
ExecBuffer execBuffer;
MM mm;

map<string,LPBYTE > dynSym;
map<string,LPBYTE > sym;
struct MySymEntry{
	string name;
	LPBYTE pdata;
};
map<pair<unsigned,unsigned>, MySymEntry > rdynSym;
map<pair<unsigned,unsigned>, MySymEntry > rsym;


int argc;
char**argv;
typedef int (*tmain)(int argc,char** argv);
void __my_start_main(tmain main, int xargc, char **xargv, void (*__libc_csu_init)(), void(*__libc_csu_fini)(),int edx, void* stack){
	int iargc;
	char**iargv;
	iargc=argc-1;
	iargv=new char*[iargc];
	for(int i=1;i<argc;++i){
		iargv[i-1]=argv[i];
	}

	cerr<<("__libc_csu_init")<<endl;
	__libc_csu_init();

	cerr<<("------------------------Call main---------------------------")<<endl;
	main(iargc,iargv);
	cerr<<"-------------------------End of main---------------------------"<<endl;
	
	cerr<<("__libc_csu_fini")<<endl;
	__libc_csu_fini();


	cerr<<("exit")<<endl;
	exit(0);
}





HMODULE hCygwin=NULL;
HMODULE libSsp=NULL;
void __stdcall __my_print_func(const char *name){
	cerr<<"*Calling: "<<name<<endl;
	cerr.flush();
}
void __stdcall __no_such_func(){
	cerr<<"No such func"<<endl;
	cerr.flush();
	exit(-1);
}
#define __never__
#ifdef __never__
unsigned short __ctype_b_a[384];
unsigned short *__ctype_b=__ctype_b_a+128;
unsigned  short * * __ctype_b_loc (void){
	return &__ctype_b;
}
#endif


void LoadSym(void *dst,string &name){
	FARPROC p=0;
	if(name=="__libc_start_main"){
		*(LPDWORD)dst= (DWORD)&__my_start_main;
		return;
	}
	if(name=="__errno_location"){
		name="__errno";
	}
	if(name=="__ctype_b_loc"){	
		p=(FARPROC)&__ctype_b_loc;
	}

	if(!p)p=GetProcAddress(hCygwin,name.c_str());
	if(!p && name.find("__isoc99_")==0){
		name=name.substr(strlen("__isoc99_"));
		p=GetProcAddress(hCygwin,name.c_str());
	}
	if(!p && name.find("_chk",name.length()-4)>=0){		
		p=GetProcAddress(libSsp,name.c_str());
	}
	
	/*
	if(!p && name[0]=='_'){
		name=name.substr(1);
		p=GetProcAddress(hCygwin,name.c_str());
	}*/
	if(!p && name.length()>2 && name[name.length()-2]=='6' &&name[name.length()-1]=='4'){
		name=name.substr(0,name.length()-2);
		p=GetProcAddress(hCygwin,name.c_str());
	}
	/*if(!p){
		p=GetProcAddress(hCygwin,('_'+name).c_str());
	}*/


	if(p==0){
		if(verbose)cout<<"Could not find symble: "<<name<<endl;
		*(LPDWORD)dst=(DWORD)__no_such_func;
	}else{
		*(LPDWORD)dst=(DWORD)p;
	}
	


	LPBYTE wrapFunc=execBuffer.Align4();
	LPBYTE real=*(LPBYTE*)dst;
	*(LPBYTE*)dst=wrapFunc;
	const char*pname=name.c_str();
	execBuffer._Push((DWORD)pname);
	execBuffer._Call((DWORD)&__my_print_func);
	execBuffer._Jmp((DWORD)real);
	execBuffer.Flush();	
}
typedef void (*VoidFunc)() ;
typedef void (*tprintf)(char *,...) ;



int domain(int argc, char* argv[])
{
	::argc=argc;
	::argv=argv;
#ifndef __CYGWIN__
	char tlspad[4096];
	hCygwin=LoadLibrary(_T("cygwin1.dll"));
	void (*init)() = (VoidFunc)GetProcAddress(hCygwin, "cygwin_dll_init");
	init();
#else
	hCygwin=GetModuleHandle(_T("cygwin1.dll"));
#endif
	libSsp=LoadLibrary(_T("cygssp-0.dll"));
	execBuffer.Init(1024*1024);

	//load file
	const TCHAR*fname=_T(argv[1]);	
	MappingFile file(fname);
	ElfFile::Ef_ehdr hdr(file.view(elfcpp::file_header_offset, ElfFile::ehdr_size).data());
	ElfFile efile(&file,hdr);
	
	//load symbols
	if(verbose)cout<<hdr.get_e_entry()<<endl;
	for(unsigned i=0;i<efile.shnum();++i){
		if(verbose)cout<<"Section:"<<efile.section_name(i)<<':'<<efile.section_contents(i).file_offset<<endl;
		ElfFile::Ef_shdr shdr(file.view(efile.section_header(i)).data());		
		switch(shdr.get_sh_type()){
		case elfcpp::SHT_DYNSYM:
			{
				if(verbose)cout<<"DYNSYM:"<<endl;
				for(DWORD s=0;s<shdr.get_sh_size();s+=shdr.get_sh_entsize()){
					LPBYTE data=(LPBYTE)file.view(efile.section_contents(i)).data()+s;
					elfcpp::Sym<32,false> ps=data;
					string name=(const char*)(file.view(efile.section_contents(shdr.get_sh_link())).data())+ps.get_st_name();
					if(verbose)cout<<'\t'<<name<<endl;
					dynSym[name]=data;
					MySymEntry sym={name,data};
					rdynSym[make_pair(i,s/shdr.get_sh_entsize())]=sym;
				}
			}
			break;
		case elfcpp::SHT_SYMTAB:
			{
				if(verbose)cout<<"SYM:"<<endl;
				for(DWORD s=0;s<shdr.get_sh_size();s+=shdr.get_sh_entsize()){
					LPBYTE data=(LPBYTE)file.view(efile.section_contents(i)).data()+s;
					elfcpp::Sym<32,false> ps=data;
					string name=(const char*)(file.view(efile.section_contents(shdr.get_sh_link())).data())+ps.get_st_name();
					if(verbose)cout<<'\t'<<name<<endl;
					sym[name]=data;
					MySymEntry sym={name,data};
					rsym[make_pair(i,s/shdr.get_sh_entsize())]=sym;
				}
			}
			break;

		}
	}

	//allocate exec memory
	for(unsigned i=0;i<hdr.get_e_phnum();++i){
		File::View v=file.view(hdr.get_e_phoff()+i*hdr.get_e_phentsize(),hdr.get_e_phentsize());
		ElfFile::Ef_phdr phdr(v.data());
		if(verbose)cout<<setbase(16)<<phdr.get_p_vaddr()<<'-'<<phdr.get_p_memsz()<<endl;
		mm.Put(phdr.get_p_paddr(),phdr.get_p_memsz());
	}
	mm.Alloc();


	//copy exec file to memory
	for(unsigned i=0;i<hdr.get_e_phnum();++i){
		File::View v=file.view(hdr.get_e_phoff()+i*hdr.get_e_phentsize(),hdr.get_e_phentsize());
		ElfFile::Ef_phdr phdr(v.data());		
		DWORD noread;
		if(phdr.get_p_filesz()){
			SetFilePointer(file.hFile,phdr.get_p_offset(),0,FILE_BEGIN);
			ReadFile(file.hFile,(LPBYTE)phdr.get_p_vaddr(),phdr.get_p_filesz(),&noread,0);			
		}
		if(phdr.get_p_flags()|elfcpp::PF_X){
			mm.Protect((LPBYTE)phdr.get_p_vaddr(),phdr.get_p_memsz(),PAGE_EXECUTE_READWRITE);
			FlushInstructionCache(GetCurrentProcess(),(LPBYTE)phdr.get_p_vaddr(),phdr.get_p_memsz());
		}
	}

	//symble relocation
	for(unsigned i=0;i<efile.shnum();++i){
		ElfFile::Ef_shdr shdr(file.view(efile.section_header(i)).data());		
		switch(shdr.get_sh_type()){
		case elfcpp::SHT_REL:
			{
				if(verbose)cout<<"SHT_REL:"<<endl;
				for(DWORD s=0;s<shdr.get_sh_size();s+=shdr.get_sh_entsize()){
					LPBYTE data=(LPBYTE)file.view(efile.section_contents(i)).data()+s;
					elfcpp::Rel<32,false> ps=data;
					switch(elfcpp::elf_r_type<32>(ps.get_r_info())){
					case elfcpp::R_386_JUMP_SLOT:
						{
							unsigned isym=elfcpp::elf_r_sym<32>(ps.get_r_info());
							string &name=rdynSym[make_pair(shdr.get_sh_link(),isym)].name;
							if(verbose)cout<<name<<endl;
							LoadSym((LPVOID)ps.get_r_offset(),name);
							break;
						}
					case elfcpp::R_386_GLOB_DAT:
						{
							unsigned isym=elfcpp::elf_r_sym<32>(ps.get_r_info());
							MySymEntry syme=rdynSym[make_pair(shdr.get_sh_link(),isym)];
							string name=syme.name;
							if(verbose)cout<<name<<endl;
							*(LPDWORD)ps.get_r_offset()=elfcpp::Sym<32,false>(syme.pdata).get_st_value();
						}
						break;
					case elfcpp::R_386_COPY:
						{
							unsigned isym=elfcpp::elf_r_sym<32>(ps.get_r_info());
							MySymEntry syme=rdynSym[make_pair(shdr.get_sh_link(),isym)];
							string name=syme.name;
							if(verbose)cout<<name<<endl;
							elfcpp::Sym<32,false>sm(syme.pdata) ;
							memcpy((LPDWORD)ps.get_r_offset(),(LPDWORD)sm.get_st_value(),sm.get_st_size());
						}
						break;
					default:
						assert(0);
						;
					}
					
				}
			}
			break;
		}
	}


	execBuffer.Flush();


	//call entry point
	typedef void (*tentry)();
	tentry entry=(tentry)hdr.get_e_entry();	

	entry();

	
	return 0;
}


int main(int argc, char* argv[]){
	if(argc<2){
		cout<<"Try ./elfloader.exe a.out"<<endl;
		return 1;
	}
	try{
		domain(argc,argv);
	}catch(char const*err){
		cout<<err<<endl;
	}catch(...){
		cout<<"Failed"<<endl;
	}
}