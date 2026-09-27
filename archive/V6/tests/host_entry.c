/* Freestanding Windows runner: the local MSVC installation lacks CRT headers. */
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
__declspec(dllimport) void * __stdcall GetStdHandle(unsigned int);
__declspec(dllimport) int __stdcall WriteFile(void*, const void*, unsigned int, unsigned int*, void*);
int puts(const char *s) {unsigned int n=0,w=0;while(s[n])n++;WriteFile(GetStdHandle((unsigned int)-11),s,n,&w,0);WriteFile(GetStdHandle((unsigned int)-11),"\n",1,&w,0);return 0;}
void test_fail(int line) {char buf[16];int n=0;puts("FAIL at source line:");do{buf[n++]=(char)('0'+line%10);line/=10;}while(line);for(int i=0;i<n/2;i++){char c=buf[i];buf[i]=buf[n-1-i];buf[n-1-i]=c;}buf[n]=0;puts(buf);ExitProcess(1);}
int main(void);
void test_entry(void) {ExitProcess((unsigned int)main());}
