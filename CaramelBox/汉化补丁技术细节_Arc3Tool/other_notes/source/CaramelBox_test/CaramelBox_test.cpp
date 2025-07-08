#include <windows.h>
#include <tchar.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	HINSTANCE hDLL;
	typedef int (__stdcall *patch_t)(char *, char *);
	patch_t patch;
	int ret;
	
	hDLL = LoadLibraryA("CaramelBox_patch");
	if (!hDLL)
		return -1;

	patch = (patch_t)GetProcAddress(hDLL, "CaramelBox_patch");	

	printf("loading CaramelBox_patch ...\n");
	ret = patch(argv[1], argv[2]);
	FreeLibrary(hDLL);

	printf("patching over ...\n");

	if (ret)
		printf("patch return %d\n", ret);

	return 0;
}
