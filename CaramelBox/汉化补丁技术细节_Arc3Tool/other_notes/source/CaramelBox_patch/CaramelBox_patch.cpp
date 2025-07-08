#include <windows.h>
#include <tchar.h>
#include <shlwapi.h>
#include <crass_types.h>
#include <stdio.h>
#include <utility.h>

/* 所有的封包特定的数据结构都要放在这个#pragma段里 */
#pragma pack (1)
typedef struct {
	s8 magic[4];			/* "arc3" */
	u32 version;			/* 0, 1, 2 */
	u32 block_size;
	u32 data_offset;		/* 以块为单位 */
	u32 total_block_nr;		/* 整个文件占用的块数 */
	u32 unknown1;
	u32 directory_offset;	/* 以块为单位 */
	u32 directory_length;	/* 实际的字节数 */
	u32 directory_block_nr;	/* 目录占用的块数 */
	u32 unknown3;
	u32 unknown4;
	u16 unknown5;
} bin_header_t;

typedef struct {			// 每个资源前面的数据头
	u32 unknown0;
	u32 length0;
	u32 length1;
	u32 unknown1;
	u32 unknown2;
	u32 type;				/* 数据编码模式 */
	u32 pad0;
	u32 pad1;
} resource_header_t;
#pragma pack ()


#define SWAP4(x)	(((x) & 0xff) << 24 | ((x) & 0xff00) << 8 | ((x) & 0xff0000) >> 8 | ((x) & 0xff000000) >> 24)
#define SWAP3(x)	(((x) & 0xff) << 16 | ((x) & 0xff00) | ((x) & 0xff0000) >> 16)
#define SWAP2(x)	((((x) & 0xff) << 8) | (((x) & 0xff00) >> 8))

static int bin_process(TCHAR *archive_name, char *resource_path, char *resource_name)
{
	HANDLE bin_handle;
	bin_header_t bin_header;
	int ret = 0;
	SIZE_T bin_size;
	DWORD directory_length, data_offset, block_size;
	BYTE *directory;
	BYTE *p_dir;
	int is_first_entry;
	DWORD entry_data_offset;
	char _archive_name[MAX_PATH];

	unicode2acp(_archive_name, MAX_PATH, archive_name, -1);
	printf("%s: patching %s ... ", PathFindFileNameA(_archive_name), resource_name);

	bin_handle = CreateFile(archive_name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (bin_handle == INVALID_HANDLE_VALUE)
		return -10;	

	if (MyReadFile(bin_handle, &bin_header, sizeof(bin_header))) {
		ret = -11;
		goto err1;
	}

	if (strncmp(bin_header.magic, "arc3", 4)) {
		ret = -98;
		goto err1;
	}

	if (SWAP4(bin_header.version) > 2) {
		ret = -98;
		goto err1;
	}

	if (MyGetFileSize(bin_handle, &bin_size)) {
		ret = -12;
		goto err1;
	}

	directory_length = SWAP4(bin_header.directory_length);
	directory = (BYTE *)malloc(directory_length);	
	if (!directory) {
		ret = -13;
		goto err1;
	}

	if (MySetFilePosition(bin_handle, SWAP4(bin_header.directory_offset) * SWAP4(bin_header.block_size), 
			NULL, FILE_BEGIN)) {
		ret = -14;
		goto err2;
	}

	if (MyReadFile(bin_handle, directory, directory_length)) {
		ret = -15;
		goto err2;
	}

	data_offset = SWAP4(bin_header.data_offset);
	block_size = SWAP4(bin_header.block_size);
	p_dir = directory;
	is_first_entry = 0;	
	while (p_dir < directory + directory_length) {
		int name_len;
		int name_offset;
		char entry_name[16];

		name_len = *p_dir & 0xf;
		name_offset = *p_dir >> 4;
		p_dir++;
		if (name_offset != 0xf) {	// 只有部分名字
			memcpy(&entry_name[name_offset], p_dir, name_len);			
			entry_name[name_offset + name_len] = 0;
			p_dir += name_len;
		} else if (name_len) {
			if (name_len == 0xf)	// 名称序号做累加
				entry_name[strlen(entry_name) - 1]++;
			else {	// 首项
				/* 首项拥有完整的名字 */
				memcpy(entry_name, p_dir, name_len);
				entry_name[name_len] = 0;				
				p_dir += name_len;
				is_first_entry = 1;
			}
		}

		if (!lstrcmpiA(entry_name, resource_name))
			break;

		p_dir += 3;

		/* 首项还要忽略到下一个首相的directory偏移字段(该字段仅在查找时使用) */
		if (is_first_entry) {
			p_dir += 3;
			is_first_entry = 0;
		}
	}
	if (p_dir < directory + directory_length) {
		resource_header_t res_header;
		TCHAR res_path[MAX_PATH];
		SIZE_T res_size;

		entry_data_offset = (SWAP3(*(u32 *)p_dir) + data_offset) * block_size;
		if (MySetFilePosition(bin_handle, entry_data_offset, NULL, FILE_BEGIN)) {
			ret = -16;
			goto err2;
		}

		if (MyReadFile(bin_handle, &res_header, sizeof(res_header))) {
			ret = -17;
			goto err2;
		}

		sj2unicode(resource_path, -1, res_path, MAX_PATH);

		HANDLE res_handle = MyOpenFile(res_path);
		if (res_handle == INVALID_HANDLE_VALUE) {
			ret = -18;
			goto err2;
		}

		DWORD act_res_size;
		if (MyGetFileSize(res_handle, &res_size)) {
			MyCloseFile(res_handle);
			ret = -19;
			goto err2;
		}
		act_res_size = res_size;

		res_size = (res_size + sizeof(res_header) + block_size - 1) & ~(block_size - 1);
		BYTE *res_data = (BYTE *)malloc(res_size);	
		if (!res_data) {
			MyCloseFile(res_handle);
			ret = -20;
			goto err2;
		}
		memset(res_data + sizeof(res_header) + act_res_size, 0, res_size - act_res_size - sizeof(res_header));

		if (MyReadFile(res_handle, res_data, act_res_size)) {
			free(res_data);
			MyCloseFile(res_handle);
			ret = -21;
			goto err2;
		}
		MyCloseFile(res_handle);

		if (MySetFilePosition(bin_handle, bin_size, NULL, FILE_BEGIN)) {
			free(res_data);
			ret = -22;
			goto err2;
		}

		/* 写入文件头 */
		res_header.length0 = res_header.length1 = SWAP4(act_res_size);
		res_header.type = 0;
		if (MyWriteFile(bin_handle, &res_header, sizeof(res_header))) {
			free(res_data);
			ret = -23;
			goto err2;
		}

		/* 写入实际数据 */
		if (MyWriteFile(bin_handle, res_data, res_size - sizeof(res_header))) {
			free(res_data);
			ret = -24;
			goto err2;
		}
		free(res_data);

		/* 修正索引项 */
		DWORD fix_entry_data_offset = bin_size / block_size - data_offset;
		*p_dir++ = ((BYTE *)&fix_entry_data_offset)[2];
		*p_dir++ = ((BYTE *)&fix_entry_data_offset)[1];
		*p_dir++ = ((BYTE *)&fix_entry_data_offset)[0];

		if (MySetFilePosition(bin_handle, SWAP4(bin_header.directory_offset) * SWAP4(bin_header.block_size), 
				NULL, FILE_BEGIN)) {
			ret = -25;
			goto err2;
		}

		if (MyWriteFile(bin_handle, directory, directory_length)) {
			ret = -26;
			goto err2;
		}

		/* TODO: 修正风封包头 */
	} else
		ret = -99;	

err2:
	free(directory);
err1:
	MyCloseFile(bin_handle);
	
	if (ret)
		printf("Failed(%d)\n", ret);
	else
		printf("OK\n");
	return ret;
}

int CALLBACK CaramelBox_patch(char *bin_path, char *resource_name)
{
	char convert_name[MAX_PATH];
	char *suffix_pos;
	char *prefix_pos;

	if (!bin_path)
		return -1;

	if (!resource_name)
		return -2;

	prefix_pos = strrchr(resource_name, '\\');
	if (!prefix_pos)
		prefix_pos = resource_name;
	else
		prefix_pos++;

	suffix_pos = strrchr(prefix_pos, '.');
	if (suffix_pos) {	// 如果有扩展名，则将名字变换为$$$name($表示后缀)
		sprintf(convert_name, "%-3s", suffix_pos + 1);
		memcpy(&convert_name[3], prefix_pos, suffix_pos - prefix_pos);
		convert_name[suffix_pos - prefix_pos + 3] = 0;
	} else				// 如果没扩展名，则补上3个空格："   name"
		sprintf(convert_name, "   %s", prefix_pos);

	if (convert_name[3] == 'X')
		convert_name[3] = '*';

	TCHAR _bin_path[MAX_PATH];
	acp2unicode(bin_path, -1, _bin_path, MAX_PATH);

	return bin_process(_bin_path, resource_name, convert_name);
}

