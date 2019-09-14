//
// Simple FIle System
// 신재협
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a, b) ((a) |= (1 << (b)))
#define BIT_CLEAR(a, b) ((a) &= ~(1 << (b)))
#define BIT_FLIP(a, b) ((a) ^= (1 << (b)))
#define BIT_CHECK(a, b) ((a) & (1 << (b)))

static struct sfs_super spb;				// superblock
static struct sfs_dir sd_cwd = {SFS_NOINO}; // current working directory
int bbnum;

struct sfs_btmp
{
	char sfb_bytes[512]; //512 바이트, 즉 8*512비트
};

int getBbnum()
{
	int bbnum_tmp, i, j;
	int swch = 0;
	int newbie_ino = -1;
	struct sfs_btmp sb;

	for (bbnum_tmp = 2; bbnum_tmp < 2 + bbnum; bbnum_tmp++)
	{							   //bitmap read
		disk_read(&sb, bbnum_tmp); //0:super 1:root 2:bitmap
		for (i = 0; i < 512; i++)  //512개의 바이트를 탐색
		{
			for (j = 0; j < 8; j++)
			{
				if (BIT_CHECK(sb.sfb_bytes[i], j) == 0) //가용 블록(bit=0) 발견
				{
					BIT_SET(sb.sfb_bytes[i], j); //해당 블록 사용시작(bit를1로)
					disk_write(&sb, bbnum_tmp);
					newbie_ino = ((SFS_BLOCKBITS * (bbnum_tmp - 2)) + (i * 8) + (j)); //새로 만들 파일의 블록번호 get!!
					swch = 1;
					break;
				}
			}
			if (swch == 1)
			{
				break;
			}
		}
		if (swch == 1)
		{
			break;
		}
	}
	return newbie_ino;
	/*if(newbie_ino==-1)
	{
		//가용 블락이 없는 경우 에러,종료
		error_message("touch",path,-4);
		return;
	}*/
}

void error_message(const char *message, const char *path, int error_code)
{
	switch (error_code)
	{
	case -1:
		printf("%s: %s: No such file or directory\n", message, path);
		return;
	case -2:
		printf("%s: %s: Not a directory\n", message, path);
		return;
	case -3:
		printf("%s: %s: Directory full\n", message, path);
		return;
	case -4:
		printf("%s: %s: No block available\n", message, path);
		return;
	case -5:
		printf("%s: %s: Not a directory\n", message, path);
		return;
	case -6:
		printf("%s: %s: Already exists\n", message, path);
		return;
	case -7:
		printf("%s: %s: Directory not empty\n", message, path);
		return;
	case -8:
		printf("%s: %s: Invalid argument\n", message, path);
		return;
	case -9:
		printf("%s: %s: Is a directory\n", message, path);
		return;
	case -10:
		printf("%s: %s: Is not a file\n", message, path);
		return;
	case -11:
		printf("%s: can't open %s input file\n", message, path);
		return;
	default:
		printf("unknown error code\n");
		return;
	}
}

void sfs_mount(const char *path)
{
	if (sd_cwd.sfd_ino != SFS_NOINO)
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	int rd;

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read(&spb, SFS_SB_LOCATION);

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert(spb.sp_magic == SFS_MAGIC);

	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);

	sd_cwd.sfd_ino = 1; //init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';

	bbnum = spb.sp_nblocks / SFS_BLOCKBITS;
	rd = spb.sp_nblocks % SFS_BLOCKBITS;
	if (rd > 0)
	{
		bbnum += 1;
	}
}

void sfs_umount()
{

	if (sd_cwd.sfd_ino != SFS_NOINO)
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}

void sfs_touch(const char *path)
{
	//skeleton implementation

	struct sfs_inode si, si_btmp, newbie; //,si_tmp;
	struct sfs_btmp sb;
	int i, j, k, l, nsd, nsd_tmp, bbnum_tmp;
	int swch = 0;
	int newbie_ino = -1;

	//we assume that cwd is the root directory and root directory is empty which has . and .. only
	//unused DISK2.img satisfy these assumption
	//for new directory entry(for new file), we use cwd.sfi_direct[0] and offset 2
	//becasue cwd.sfi_directory[0] is already allocated, by .(offset 0) and ..(offset 1)
	//for new inode, we use block 6
	// block 0: superblock,	block 1:root, 	block 2:bitmap
	// block 3:bitmap,  	block 4:bitmap 	block 5:root.sfi_direct[0] 	block 6:unused
	//
	//if used DISK2.img is used, result is not defined

	//buffer for disk read
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd_real[SFS_DENTRYPERBLOCK]; //디렉토리 엔트리 배열

	disk_read(&si, sd_cwd.sfd_ino); //현재 디렉토리의 inode정보 read
	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR); //si가 디렉토리의 inode인지

	nsd = si.sfi_size / sizeof(struct sfs_dir); //nsd=si.sfi_size/sizeof(struct sfs_dir); //nsd은 해당 inode의 디렉토리 엔트리 수
	k = nsd / SFS_DENTRYPERBLOCK;				//k번째 다이렉트 사용, SFS_DENTRYPERBLOCK=8
	l = nsd % SFS_DENTRYPERBLOCK;				//k번째 다이렉트에 l개 엔트리 있음

	//spb.sp_nblocks++; //전체 데이터블록 1개 추가
	if (k >= SFS_NDIRECT)
	{
		//할당 받을 디렉토리 엔트리가 없는 경우
		error_message("touch", path, -3);
		return;
	}

	if (l == 0)
	{
		si.sfi_direct[k] = getBbnum();
		if (si.sfi_direct[k] == -1)
		{
			//가용 블락이 없는 경우 에러,종료
			error_message("touch", path, -4);
			return;
		}
		disk_read(sd, si.sfi_direct[k]); //현재 디렉토리의 디렉토리엔트리들 read
		for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
		{
			sd[i].sfd_ino = SFS_NOINO;
			sd[i].sfd_name[0] = '\0';
		}
		disk_write(sd, si.sfi_direct[k]);
		//spb.sp_nblocks++; //전체 데이터블록 1개 추가
	}

	for (j = 0; j < k; j++)
	{
		//block access
		disk_read(sd, si.sfi_direct[j]); //현재 디렉토리의 디렉토리엔트리들 read

		for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
		{
			//disk_read( &si_tmp,sd[i].sfd_ino );
			if (!strcmp(sd[i].sfd_name, path)) //만들고자 하는 파일과 같은 이름 발견
			{
				//같은 이름이 있으므로 에러
				error_message("touch", path, -6);
				return;
			}
		}
	}
	bzero(sd, SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir));

	//block access
	disk_read(sd, si.sfi_direct[k]); //현재 디렉토리의 디렉토리엔트리들 read

	for (i = 0; i < l; i++)
	{
		//disk_read( &si_tmp,sd[i].sfd_ino );
		if (!strcmp(sd[i].sfd_name, path)) //만들고자 하는 파일과 같은 이름 발견
		{
			//같은 이름이 있으므로 에러
			error_message("touch", path, -6);
			return;
		}
	}
	bzero(sd, SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir));

	newbie_ino = getBbnum();
	if (newbie_ino == -1)
	{
		//가용 블락이 없는 경우 에러,종료
		error_message("touch", path, -4);
		return;
	}

	//printf("nsd:%d\tk:%d\tl:%d\n",nsd,k,l);
	//block access
	disk_read(sd_real, si.sfi_direct[k]); //현재 디렉토리의 디렉토리엔트리들 read

	sd_real[l].sfd_ino = newbie_ino;
	strncpy(sd_real[l].sfd_name, path, SFS_NAMELEN); //(복사받는 문자열, 복사할 문자열)
	disk_write(sd_real, si.sfi_direct[k]);			 //수정된 sd를 si 디렉토리 엔트리에 write

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write(&si, sd_cwd.sfd_ino);

	bzero(&newbie, SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;
	disk_write(&newbie, newbie_ino);
}

//1단계1
void sfs_cd(const char *path)
{
	//printf("Not Implemented\n");

	if (path == NULL)
	{
		sd_cwd.sfd_ino = 1; //init at root
		sd_cwd.sfd_name[0] = '/';
		sd_cwd.sfd_name[1] = '\0';
		return;
	}
	else //현재(sd_cwd) 디렉토리에서 해당path 탐색
	{
		int i, j, k, l, m, j2, k2, l2, nsd, nsd_tmp, inodenum = sd_cwd.sfd_ino;

		//buffer for disk read
		struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd_tmp[SFS_DENTRYPERBLOCK]; //sfs_dir구조체 배열 = 블록
		struct sfs_inode si, si_tmp;
		int idx = -1;
		u_int32_t qarr[999]; //BFS를 위해 큐처럼 사용될 배열 qarr

		qarr[++idx] = inodenum; //푸쉬. q.push(inodenum);
		while (idx > -1)		//!q.empty()
		{
			disk_read(&si, qarr[idx] /*q.top()*/); //디스크로부터 디렉토리의 inode 읽어옴
			if (idx > -1)						   //팝. q.pop();
			{
				qarr[idx--] = -1;
			}
			//for consistency
			assert(si.sfi_type == SFS_TYPE_DIR); //디렉토리 타입인지 판별. 아니라면 종료됨.

			nsd = si.sfi_size / sizeof(struct sfs_dir); //nsd=si.sfi_size/sizeof(struct sfs_dir); //nsd은 해당 inode의 디렉토리 엔트리 수
			k = nsd / SFS_DENTRYPERBLOCK;				//k번째 다이렉트 사용
			l = nsd % SFS_DENTRYPERBLOCK;				//k번째 다이렉트에 l개 엔트리 있음

			for (j = 0; j < k; j++)
			{
				//block access
				disk_read(sd, si.sfi_direct[j]); //해당 inode의 블록을 sd로 가져옴. sd[0]은 현재 디렉토리,sd[1]은 부모 디렉토리

				for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
				{
					disk_read(&si_tmp, sd[i].sfd_ino);
					if (!strcmp(sd[i].sfd_name, path))
					{
						if (si_tmp.sfi_type == SFS_TYPE_DIR)
						{
							sd_cwd.sfd_ino = sd[i].sfd_ino;
							strcpy(sd_cwd.sfd_name, sd[i].sfd_name); //path에 해당하는 디렉토리 이름을 sd_cwd에 복사
						}
						else if (si_tmp.sfi_type == SFS_TYPE_FILE) //디렉토리가 아님
						{
							error_message("cd", path, -2);
						}
						return;
					}
				}
			}

			//block access
			disk_read(sd, si.sfi_direct[k]); //해당 inode의 블록을 sd로 가져옴. sd[0]은 현재 디렉토리,sd[1]은 부모 디렉토리

			for (i = 0; i < l; i++)
			{
				disk_read(&si_tmp, sd[i].sfd_ino);
				if (!strcmp(sd[i].sfd_name, path))
				{
					if (si_tmp.sfi_type == SFS_TYPE_DIR)
					{
						sd_cwd.sfd_ino = sd[i].sfd_ino;
						strcpy(sd_cwd.sfd_name, sd[i].sfd_name); //path에 해당하는 디렉토리 이름을 sd_cwd에 복사
					}
					else if (si_tmp.sfi_type == SFS_TYPE_FILE) //디렉토리가 아님
					{
						error_message("cd", path, -2);
					}
					return;
				}
			}
		}

		//해당 디렉토리가 없음
		error_message("cd", path, -1);
	}
}

//1단계2
void sfs_ls(const char *path) //매개변수: path에 대해 ls 해당하는 path 없으면 에러메시지
{
	//printf("sizeof(struct sfs_dir)=%dB\n",sizeof(struct sfs_dir));
	//printf("//// 현재spb.sp_magic = %X\n",spb.sp_magic);
	//printf("Not Implemented\n");
	if (path == NULL)
	{
		int i, j, k, l, nsd;
		struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd_tmp[SFS_DENTRYPERBLOCK]; //sfs_dir구조체 배열 = 블록
		struct sfs_inode si, si_tmp;

		disk_read(&si, sd_cwd.sfd_ino); //디스크로부터 현재 디렉터리의 inode 읽어옴
		//for consistency
		assert(si.sfi_type == SFS_TYPE_DIR); //유효한 inode인지 판별. 아니라면 종료됨.

		nsd = si.sfi_size / sizeof(struct sfs_dir); //nsd=si.sfi_size/sizeof(struct sfs_dir); //nsd은 해당 inode의 디렉토리 엔트리 수
		k = nsd / SFS_DENTRYPERBLOCK;				//k-1번째 다이렉트 사용
		l = nsd % SFS_DENTRYPERBLOCK;				//k-1번째 다이렉트에 l개 엔트리 있음

		for (j = 0; j < k; j++)
		{
			//block access
			disk_read(sd, si.sfi_direct[j]); //해당 inode의 블록을 sd로 가져옴. sd[0]은 현재 디렉토리,sd[1]은 부모 디렉토리

			for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
			{
				disk_read(&si_tmp, sd[i].sfd_ino);
				if (si_tmp.sfi_type == SFS_TYPE_DIR) //디렉토리인 경우 끝에 /출력
				{
					printf("%s/\t", sd[i].sfd_name);
				}
				else if (si_tmp.sfi_type == SFS_TYPE_FILE) //파일
				{
					printf("%s\t", sd[i].sfd_name);
				}
			}
		}

		//block access
		disk_read(sd, si.sfi_direct[k]); //해당 inode의 블록을 sd로 가져옴. sd[0]은 현재 디렉토리,sd[1]은 부모 디렉토리

		for (i = 0; i < l; i++)
		{
			disk_read(&si_tmp, sd[i].sfd_ino);
			if (si_tmp.sfi_type == SFS_TYPE_DIR) //디렉토리인 경우 끝에 /출력
			{
				printf("%s/\t", sd[i].sfd_name);
			}
			else if (si_tmp.sfi_type == SFS_TYPE_FILE) //파일
			{
				printf("%s\t", sd[i].sfd_name);
			}
		}
		printf("\n");

		return;
	}
	//현재(sd_cwd)디렉토리에서 해당path 탐색

	int i, j, k, l, m, j2, k2, l2, nsd, nsd_tmp, inodenum = sd_cwd.sfd_ino;

	//buffer for disk read
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd_tmp[SFS_DENTRYPERBLOCK]; //sfs_dir구조체 배열 = 블록
	struct sfs_inode si, si_tmp, si_tmp2;
	int idx = -1, swch = 0;
	u_int32_t qarr[999]; //BFS를 위해 큐처럼 사용될 배열 qarr

	qarr[++idx] = inodenum; //푸쉬. q.push(inodenum);
	while (idx > -1)
	{
		disk_read(&si, qarr[idx] /*q.top()*/); //디스크로부터 디렉토리의 inode 읽어옴
		if (idx > -1)						   //팝. q.pop();
		{
			qarr[idx--] = -1;
		}
		//for consistency
		assert(si.sfi_type == SFS_TYPE_DIR); //디렉토리 타입인지 판별. 아니라면 종료됨.

		nsd = si.sfi_size / sizeof(struct sfs_dir); //nsd=si.sfi_size/sizeof(struct sfs_dir); //nsd은 해당 inode의 디렉토리 엔트리 수
		k = nsd / SFS_DENTRYPERBLOCK;				//k번째 다이렉트 사용
		l = nsd % SFS_DENTRYPERBLOCK;				//k번째 다이렉트에 l개 엔트리 있음

		for (j = 0; j < k; j++)
		{
			disk_read(sd, si.sfi_direct[j]);
			for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
			{
				disk_read(&si_tmp, sd[i].sfd_ino);
				if (!strcmp(sd[i].sfd_name, path)) //원하는 디렉토리 찾음
				{
					//block access
					//disk_read( sd_tmp, si_tmp.sfi_direct[j] ); //해당 inode의 블록을 sd로 가져옴. sd[0]은 현재 디렉토리,sd[1]은 부모 디렉토리

					if (si_tmp.sfi_type == SFS_TYPE_FILE) //st_tmp가 파일의 inode라면 파일 이름 출력
					{
						printf("%s\n", sd[i].sfd_name);
						return;
					}
					swch = 1;

					nsd_tmp = si_tmp.sfi_size / sizeof(struct sfs_dir); //sizeof(sd_tmp)/sizeof(struct sfs_dir);
					k2 = nsd_tmp / SFS_DENTRYPERBLOCK;
					l2 = nsd_tmp % SFS_DENTRYPERBLOCK;

					for (j2 = 0; j2 < k2; j2++)
					{
						disk_read(sd_tmp, si_tmp.sfi_direct[j2]);
						for (m = 0; m < SFS_DENTRYPERBLOCK; m++)
						{
							disk_read(&si_tmp2, sd_tmp[m].sfd_ino);
							if (si_tmp2.sfi_type == SFS_TYPE_DIR) //디렉토리인 경우 끝에 /출력
							{
								printf("%s/\t", sd_tmp[m].sfd_name);
							}
							else if (si_tmp2.sfi_type == SFS_TYPE_FILE) //파일
							{
								printf("%s\t", sd_tmp[m].sfd_name);
							}
						}
					}

					disk_read(sd_tmp, si_tmp.sfi_direct[k2]);
					for (m = 0; m < l2; m++)
					{
						disk_read(&si_tmp2, sd_tmp[m].sfd_ino);
						if (si_tmp2.sfi_type == SFS_TYPE_DIR) //디렉토리인 경우 끝에 /출력
						{
							printf("%s/\t", sd_tmp[m].sfd_name);
						}
						else if (si_tmp2.sfi_type == SFS_TYPE_FILE) //파일
						{
							printf("%s\t", sd_tmp[m].sfd_name);
						}
					}
					printf("\n");
				}
			}
		}

		//block access
		disk_read(sd, si.sfi_direct[k]); //해당 inode의 블록을 sd로 가져옴. sd[0]은 현재 디렉토리,sd[1]은 부모 디렉토리

		for (i = 0; i < l; i++)
		{
			disk_read(&si_tmp, sd[i].sfd_ino);
			if (!strcmp(sd[i].sfd_name, path)) //원하는 디렉토리 찾음
			{
				swch = 1;
				if (si_tmp.sfi_type == SFS_TYPE_FILE) //st_tmp가 파일의 inode라면 파일 이름 출력
				{
					printf("%s\n", sd[i].sfd_name);
					return;
				}
				nsd_tmp = si_tmp.sfi_size / sizeof(struct sfs_dir); //sizeof(sd_tmp)/sizeof(struct sfs_dir);
				k2 = nsd_tmp / SFS_DENTRYPERBLOCK;
				l2 = nsd_tmp % SFS_DENTRYPERBLOCK;

				for (j2 = 0; j2 < k2; j2++)
				{
					disk_read(sd_tmp, si_tmp.sfi_direct[j2]);
					for (m = 0; m < SFS_DENTRYPERBLOCK; m++)
					{
						disk_read(&si_tmp2, sd_tmp[m].sfd_ino);
						if (si_tmp2.sfi_type == SFS_TYPE_DIR) //디렉토리인 경우 끝에 /출력
						{
							printf("%s/\t", sd_tmp[m].sfd_name);
						}
						else if (si_tmp2.sfi_type == SFS_TYPE_FILE) //파일
						{
							printf("%s\t", sd_tmp[m].sfd_name);
						}
					}
				}

				disk_read(sd_tmp, si_tmp.sfi_direct[k2]);
				for (m = 0; m < l2; m++)
				{
					disk_read(&si_tmp2, sd_tmp[m].sfd_ino);
					if (si_tmp2.sfi_type == SFS_TYPE_DIR) //디렉토리인 경우 끝에 /출력
					{
						printf("%s/\t", sd_tmp[m].sfd_name);
					}
					else if (si_tmp2.sfi_type == SFS_TYPE_FILE) //파일
					{
						printf("%s\t", sd_tmp[m].sfd_name);
					}
				}
				printf("\n");
			}
		}
	}
	//해당 디렉토리나 파일이 없음
	if (swch == 0)
	{
		error_message("ls", path, -1);
	}
}

void sfs_mkdir(const char *org_path)
{
	//printf("Not Implemented\n");

	int i, j, k, l, nsd, bbnum_tmp, parent_ino;
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd_tmp[SFS_DENTRYPERBLOCK], sd_tmp2[SFS_DENTRYPERBLOCK]; //sfs_dir구조체 배열 = 블록
	struct sfs_inode si, si_tmp;
	struct sfs_btmp sb;
	int swch = 0;
	int newbie_ino = -1;

	disk_read(&si, sd_cwd.sfd_ino); //디스크로부터 현재 디렉터리의 inode 읽어옴
	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR); //유효한 inode인지 판별. 아니라면 종료됨.

	nsd = si.sfi_size / sizeof(struct sfs_dir); //nsd=si.sfi_size/sizeof(struct sfs_dir); //nsd은 해당 inode의 디렉토리 엔트리 수
	k = nsd / SFS_DENTRYPERBLOCK;				//k번째 다이렉트 사용
	l = nsd % SFS_DENTRYPERBLOCK;				//k번째 다이렉트에 l개 엔트리 있음

	disk_read(sd, si.sfi_direct[0]);
	parent_ino = sd[0].sfd_ino;

	for (j = 0; j < k; j++)
	{
		disk_read(sd, si.sfi_direct[j]); //현재 디렉토리의 디렉토리엔트리들 read

		for (i = 0; i < SFS_DENTRYPERBLOCK; i++) //같은 이름의 파일이나 디렉토리가 있는지 찾는 loop
		{
			//disk_read( &si_tmp, sd[i].sfd_ino );
			if (!strcmp(sd[i].sfd_name, org_path))
			{
				//같은 이름의 파일이나 디렉토리가 있음
				error_message("mkdir", org_path, -6);
				return;
			}
		}
	}

	if (l > 0)
	{									 //block access
										 //disk_read( sd, si.sfi_direct[0] ); //해당 inode의 direct배열을 sd로 가져옴. sd[0]은 현재 디렉토리,sd[1]은 부모 디렉토리
		disk_read(sd, si.sfi_direct[k]); //현재 디렉토리의 디렉토리엔트리들 read

		for (i = 0; i < l; i++) //같은 이름의 파일이나 디렉토리가 있는지 찾는 loop
		{
			//disk_read( &si_tmp, sd[i].sfd_ino );
			if (!strcmp(sd[i].sfd_name, org_path))
			{
				//같은 이름의 파일이나 디렉토리가 있음
				error_message("mkdir", org_path, -6);
				return;
			}
		}
	}

	if (k >= SFS_NDIRECT)
	{
		//할당 받을 디렉토리 엔트리가 없는 경우
		error_message("mkdir", org_path, -3);
		return;
	}

	if (l == 0)
	{
		si.sfi_direct[k] = getBbnum();
		if (si.sfi_direct[k] == -1)
		{
			//가용 블락이 없는 경우 에러,종료
			error_message("mkdir", org_path, -4);
			return;
		}
		disk_read(sd, si.sfi_direct[k]); //현재 디렉토리의 디렉토리엔트리들 read
		bzero(&sd, SFS_DENTRYPERBLOCK);  // initalize sfi_direct[] and sfi_indirect
		for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
		{
			sd[i].sfd_ino = SFS_NOINO;
			sd[i].sfd_name[0] = '\0';
		}
		disk_write(sd, si.sfi_direct[k]);
		//spb.sp_nblocks++; //전체 데이터블록 1개 추가
	}

	newbie_ino = getBbnum();
	if (newbie_ino == -1)
	{
		//가용 블락이 없는 경우 에러,종료
		error_message("mkdir", org_path, -4);
		return;
	}

	//부모 디텍토리에 대한 조정 코드 st
	si.sfi_size += sizeof(struct sfs_dir);
	disk_write(&si, sd_cwd.sfd_ino);

	disk_read(sd, si.sfi_direct[k]); //현재 디렉토리의 디렉토리엔트리들 read
	sd[l].sfd_ino = newbie_ino;		 //새로 할당된 디렉토리의 inode#할당
	//strcpy(sd[i].sfd_name,org_path);
	strncpy(sd[l].sfd_name, org_path, SFS_NAMELEN); //(복사받는 문자열, 복사할 문자열)
	disk_write(sd, si.sfi_direct[k]);
	//spb.sp_nblocks++; //전체 데이터블록 1개 추가
	//부모 디텍토리에 대한 조정 코드 ed

	//새로 생긴 디렉토리에 대한 조정코드 st
	disk_read(&si_tmp, sd[l].sfd_ino);
	bzero(&si_tmp, SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	si_tmp.sfi_size = 2 * (sizeof(struct sfs_dir));
	si_tmp.sfi_type = SFS_TYPE_DIR;
	si_tmp.sfi_direct[0] = getBbnum();
	if (si_tmp.sfi_direct[0] == -1)
	{
		//가용 블락이 없는 경우 에러,종료
		error_message("mkdir", org_path, -4);
		return;
	}
	//spb.sp_nblocks++; //전체 데이터블록 1개 추가
	disk_write(&si_tmp, sd[l].sfd_ino);

	disk_read(sd_tmp, si_tmp.sfi_direct[0]);
	bzero(&sd_tmp, SFS_DENTRYPERBLOCK); // initalize sfi_direct[] and sfi_indirect
	for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
	{
		sd_tmp[i].sfd_ino = SFS_NOINO;
		sd_tmp[i].sfd_name[0] = '\0';
	}
	sd_tmp[0].sfd_ino = sd[l].sfd_ino;
	sd_tmp[0].sfd_name[0] = '.';
	sd_tmp[0].sfd_name[1] = '\0';
	sd_tmp[1].sfd_ino = parent_ino;
	sd_tmp[1].sfd_name[0] = '.';
	sd_tmp[1].sfd_name[1] = '.';
	sd_tmp[1].sfd_name[2] = '\0';
	for (i = 2; i < SFS_DENTRYPERBLOCK; i++)
	{
		sd_tmp[i].sfd_ino = SFS_NOINO;
		sd_tmp[i].sfd_name[0] = '\0';
	}
	disk_write(sd_tmp, si_tmp.sfi_direct[0]);
	//새로 생긴 디렉토리에 대한 조정코드 ed

	return;
}

void sfs_rmdir(const char *org_path)
{
	//printf("Not Implemented\n");

	//현재(sd_cwd)디렉토리에서 org_path 탐색

	int i, j, k, l, nsd, nsd_tmp, inodenum = sd_cwd.sfd_ino;

	//buffer for disk read
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd_tmp[SFS_DENTRYPERBLOCK]; //sfs_dir구조체 배열 = 블록
	struct sfs_inode si, si_tmp, si_tmp2;
	int idx = -1;
	u_int32_t qarr[999]; //BFS를 위해 큐처럼 사용될 배열 qarr

	qarr[++idx] = inodenum; //푸쉬. q.push(inodenum);
	while (idx > -1)
	{
		disk_read(&si, qarr[idx] /*q.top()*/); //디스크로부터 디렉토리의 inode 읽어옴
		if (idx > -1)						   //팝. q.pop();
		{
			qarr[idx--] = -1;
		}
		//for consistency
		assert(si.sfi_type == SFS_TYPE_DIR); //유효한 inode인지 판별. 아니라면 종료됨.

		nsd = si.sfi_size / sizeof(struct sfs_dir); //nsd=si.sfi_size/sizeof(struct sfs_dir); //nsd은 해당 inode의 디렉토리 엔트리 수
		k = nsd / SFS_DENTRYPERBLOCK;				//k-1번째 다이렉트 사용
		l = nsd % SFS_DENTRYPERBLOCK;				//k-1번째 다이렉트에 l개 엔트리 있음

		for (j = 0; j < k; j++)
		{
			//block access
			disk_read(sd, si.sfi_direct[j]); //현재 디렉토리 엔트리 배열을 sd로 가져옴. sd[0]은 현재 디렉토리,sd[1]은 부모 디렉토리

			for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
			{
				disk_read(&si_tmp, sd[i].sfd_ino);
				if (!strcmp(sd[i].sfd_name, org_path)) //원하는 디렉토리 찾음
				{
					//block access
					//disk_read( sd_tmp, si_tmp.sfi_direct[0] );

					if (si_tmp.sfi_type == SFS_TYPE_FILE)
					{
						//해당 org_path 디렉토리가 아님
						error_message("rmdir", org_path, -2);
						return;
					}
					else if (si_tmp.sfi_type == SFS_TYPE_DIR)
					{
						if (si_tmp.sfi_size / sizeof(struct sfs_dir) > 2)
						{
							//해당 디렉토리가 안 비어있음
							error_message("rmdir", org_path, -7);
							return;
						}
						else
						{
							int k, bbnum_tmp;
							int sd_ino = sd[i].sfd_ino;
							int qt, rd; //비트맵에서 해당 비트를 찾기 위한 ino의 몫qt과 나머지rd
							struct sfs_btmp sb;

							bbnum_tmp = sd[i].sfd_ino / SFS_BLOCKBITS;
							bbnum_tmp += 2;

							disk_read(&sb, bbnum_tmp); //0:super 1:root 2:bitmap
							qt = sd[i].sfd_ino / 8;
							rd = sd[i].sfd_ino % 8;
							BIT_CLEAR(sb.sfb_bytes[qt], rd);
							disk_write(&sb, bbnum_tmp);

							si_tmp.sfi_size = 0;			  //지우고자하는 디렉토리의 크기 0으로
							si_tmp.sfi_type = SFS_TYPE_INVAL; //지우고자하는 파일타입 inval
							for (k = 0; k < SFS_NDIRECT; k++)
							{
								//파일의 모든 direct데이터블록 릴리즈
								if (si_tmp.sfi_direct[k] > 0)
								{
									bbnum_tmp = si_tmp.sfi_direct[k] / SFS_BLOCKBITS;
									bbnum_tmp += 2;

									disk_read(&sb, bbnum_tmp); //0:super 1:root 2~:bitmap
									qt = si_tmp.sfi_direct[k] / 8;
									rd = si_tmp.sfi_direct[k] % 8;
									BIT_CLEAR(sb.sfb_bytes[qt], rd);
									disk_write(&sb, bbnum_tmp);
								}
								si_tmp.sfi_direct[k] = SFS_NOINO;
							}
							si_tmp.sfi_indirect = SFS_NOINO; //디렉토리의 indirect데이터블록 릴리즈

							disk_write(&si_tmp, sd_ino);

							for (k = i; k < nsd - 1; k++)
							{
								sd[k].sfd_ino = sd[k + 1].sfd_ino;
								strncpy(sd[k].sfd_name, sd[k + 1].sfd_name, SFS_NAMELEN); //(복사받는 문자열, 복사할 문자열)
							}
							sd[nsd - 1].sfd_ino = SFS_NOINO;
							sd[nsd - 1].sfd_name[0] = '\0';
							disk_write(sd, si.sfi_direct[0]);

							si.sfi_size -= sizeof(struct sfs_dir);
							disk_write(&si, sd_cwd.sfd_ino);

							//spb.sp_nblocks--;
							return;
						}
					}
				}
			}
		}

		//block access
		disk_read(sd, si.sfi_direct[k]); //현재 디렉토리 엔트리 배열을 sd로 가져옴. sd[0]은 현재 디렉토리,sd[1]은 부모 디렉토리

		for (i = 0; i < l; i++)
		{
			disk_read(&si_tmp, sd[i].sfd_ino);
			if (!strcmp(sd[i].sfd_name, org_path)) //원하는 디렉토리 찾음
			{
				//block access
				//disk_read( sd_tmp, si_tmp.sfi_direct[0] );

				if (si_tmp.sfi_type == SFS_TYPE_FILE)
				{
					//해당 org_path 디렉토리가 아님
					error_message("rmdir", org_path, -2);
					return;
				}
				else if (si_tmp.sfi_type == SFS_TYPE_DIR)
				{
					if (si_tmp.sfi_size / sizeof(struct sfs_dir) > 2)
					{
						//해당 디렉토리가 안 비어있음
						error_message("rmdir", org_path, -7);
						return;
					}
					else
					{
						int k, bbnum_tmp;
						int sd_ino = sd[i].sfd_ino;
						int qt, rd; //비트맵에서 해당 비트를 찾기 위한 ino의 몫qt과 나머지rd
						struct sfs_btmp sb;

						bbnum_tmp = sd[i].sfd_ino / SFS_BLOCKBITS;
						bbnum_tmp += 2;

						disk_read(&sb, bbnum_tmp); //0:super 1:root 2:bitmap
						qt = sd[i].sfd_ino / 8;
						rd = sd[i].sfd_ino % 8;
						BIT_CLEAR(sb.sfb_bytes[qt], rd);
						disk_write(&sb, bbnum_tmp);

						si_tmp.sfi_size = 0;			  //지우고자하는 파일의 크기 0으로
						si_tmp.sfi_type = SFS_TYPE_INVAL; //지우고자하는 파일타입 inval
						for (k = 0; k < SFS_NDIRECT; k++)
						{
							//파일의 모든 direct데이터블록 릴리즈
							if (si_tmp.sfi_direct[k] > 0)
							{
								bbnum_tmp = si_tmp.sfi_direct[k] / SFS_BLOCKBITS;
								bbnum_tmp += 2;

								disk_read(&sb, bbnum_tmp); //0:super 1:root 2~:bitmap
								qt = si_tmp.sfi_direct[k] / 8;
								rd = si_tmp.sfi_direct[k] % 8;
								BIT_CLEAR(sb.sfb_bytes[qt], rd);
								disk_write(&sb, bbnum_tmp);
							}
							si_tmp.sfi_direct[k] = SFS_NOINO;
						}
						si_tmp.sfi_indirect = SFS_NOINO; //디렉토리의 indirect데이터블록 릴리즈

						disk_write(&si_tmp, sd_ino);

						for (k = i; k < nsd - 1; k++)
						{
							sd[k].sfd_ino = sd[k + 1].sfd_ino;
							strncpy(sd[k].sfd_name, sd[k + 1].sfd_name, SFS_NAMELEN); //(복사받는 문자열, 복사할 문자열)
						}
						sd[nsd - 1].sfd_ino = SFS_NOINO;
						sd[nsd - 1].sfd_name[0] = '\0';
						disk_write(sd, si.sfi_direct[0]);

						si.sfi_size -= sizeof(struct sfs_dir);
						disk_write(&si, sd_cwd.sfd_ino);

						//spb.sp_nblocks--;
						return;
					}
				}
			}
		}
	}
	//해당 디렉토리가 없음
	error_message("rmdir", org_path, -1);
}

void sfs_mv(const char *src_name, const char *dst_name)
{
	//printf("Not Implemented\n");

	int i, j, k, l, nsd, inodenum = sd_cwd.sfd_ino;

	//buffer for disk read
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd_tmp[SFS_DENTRYPERBLOCK]; //sfs_dir구조체 배열 = 블록
	struct sfs_inode si, si_tmp;
	int idx = -1;
	u_int32_t qarr[999]; //BFS를 위해 큐처럼 사용될 배열 qarr

	qarr[++idx] = inodenum; //푸쉬. q.push(inodenum);
	while (idx > -1)		//!q.empty()
	{
		disk_read(&si, qarr[idx] /*q.top()*/); //디스크로부터 디렉토리의 inode 읽어옴
		if (idx > -1)						   //팝. q.pop();
		{
			qarr[idx--] = -1;
		}
		//for consistency
		assert(si.sfi_type == SFS_TYPE_DIR); //디렉토리 타입인지 판별. 아니라면 종료됨.

		nsd = si.sfi_size / sizeof(struct sfs_dir); //nsd=si.sfi_size/sizeof(struct sfs_dir); //nsd은 해당 inode의 디렉토리 엔트리 수
		k = nsd / SFS_DENTRYPERBLOCK;				//k-1번째 다이렉트 사용
		l = nsd % SFS_DENTRYPERBLOCK;				//k-1번째 다이렉트에 l개 엔트리 있음

		bzero(sd, SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir));

		for (j = 0; j < k; j++)
		{
			//block access
			disk_read(sd, si.sfi_direct[j]); //해당 inode의 블록을 sd로 가져옴. sd[0]은 현재 디렉토리,sd[1]은 부모 디렉토리

			for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
			{
				//disk_read( &si_tmp,sd[i].sfd_ino );
				if (!strcmp(sd[i].sfd_name, dst_name))
				{
					//dst_name가 이미 존재하는 경우가 있으면 에러메시지
					error_message("mv", dst_name, -6);
					return;
				}
			}
		}
		bzero(sd, SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir));
		//block access
		disk_read(sd, si.sfi_direct[k]); //해당 inode의 블록을 sd로 가져옴. sd[0]은 현재 디렉토리,sd[1]은 부모 디렉토리
		for (i = 0; i < l; i++)
		{
			//disk_read( &si_tmp,sd[i].sfd_ino );
			if (!strcmp(sd[i].sfd_name, dst_name))
			{
				//dst_name가 이미 존재하는 경우가 있으면 에러메시지
				error_message("mv", dst_name, -6);
				return;
			}
		}
		bzero(sd, SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir));

		//src_name의 이름을 dst_name의 이름으로 바꿈
		nsd = si.sfi_size / sizeof(struct sfs_dir); //nsd=si.sfi_size/sizeof(struct sfs_dir); //nsd은 해당 inode의 디렉토리 엔트리 수
		k = nsd / SFS_DENTRYPERBLOCK;				//k-1번째 다이렉트 사용
		l = nsd % SFS_DENTRYPERBLOCK;				//k-1번째 다이렉트에 l개 엔트리 있음

		for (j = 0; j < k; j++)
		{
			disk_read(sd, si.sfi_direct[j]);
			for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
			{
				//disk_read( &si_tmp,sd[i].sfd_ino );
				if (!strcmp(sd[i].sfd_name, src_name))
				{
					bzero(sd[i].sfd_name, SFS_NAMELEN);
					//strcpy(sd[i].sfd_name,dst_name); //dst_name에 해당하는 디렉토리 이름을 sd_cwd에 복사
					strncpy(sd[i].sfd_name, dst_name, SFS_NAMELEN); //(복사받는 문자열, 복사할 문자열)
					disk_write(sd, si.sfi_direct[j]);
					disk_write(&si, sd_cwd.sfd_ino);
					return;
				}
			}
		}
		bzero(sd, SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir));
		disk_read(sd, si.sfi_direct[k]);
		for (i = 0; i < l; i++)
		{
			//disk_read( &si_tmp,sd[i].sfd_ino );
			if (!strcmp(sd[i].sfd_name, src_name))
			{
				bzero(sd[i].sfd_name, SFS_NAMELEN);
				//strcpy(sd[i].sfd_name,dst_name); //dst_name에 해당하는 디렉토리 이름을 sd_cwd에 복사
				strncpy(sd[i].sfd_name, dst_name, SFS_NAMELEN); //(복사받는 문자열, 복사할 문자열)
				disk_write(sd, si.sfi_direct[k]);
				disk_write(&si, sd_cwd.sfd_ino);
				return;
			}
		}
		bzero(sd, SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir));
	}

	//해당 디렉토리가 없음
	error_message("mv", src_name, -1);
}

void sfs_rm(const char *path)
{
	//현재(sd_cwd)디렉토리에서 path 탐색

	int i, j, k, l, nsd, nsd_tmp, inodenum = sd_cwd.sfd_ino;

	//buffer for disk read
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd_tmp[SFS_DENTRYPERBLOCK]; //sfs_dir구조체 배열 = 블록
	struct sfs_inode si, si_tmp, si_tmp2;
	int idx = -1;
	u_int32_t qarr[999]; //BFS를 위해 큐처럼 사용될 배열 qarr

	qarr[++idx] = inodenum; //푸쉬. q.push(inodenum);
	while (idx > -1)
	{
		disk_read(&si, qarr[idx] /*q.top()*/); //디스크로부터 디렉토리의 inode 읽어옴
		if (idx > -1)						   //팝. q.pop();
		{
			qarr[idx--] = -1;
		}
		//for consistency
		assert(si.sfi_type == SFS_TYPE_DIR); //유효한 inode인지 판별. 아니라면 종료됨.

		nsd = si.sfi_size / sizeof(struct sfs_dir); //nsd=si.sfi_size/sizeof(struct sfs_dir); //nsd은 해당 inode의 디렉토리 엔트리 수
		k = nsd / SFS_DENTRYPERBLOCK;				//k-1번째 다이렉트 사용
		l = nsd % SFS_DENTRYPERBLOCK;				//k-1번째 다이렉트에 l개 엔트리 있음

		for (j = 0; j < k; j++)
		{
			//block access
			disk_read(sd, si.sfi_direct[j]); //해당 inode의 블록을 sd로 가져옴. sd[0]은 현재 디렉토리,sd[1]은 부모 디렉토리

			for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
			{
				disk_read(&si_tmp, sd[i].sfd_ino);
				if (!strcmp(sd[i].sfd_name, path)) //원하는 디렉토리 찾음
				{
					if (si_tmp.sfi_type == SFS_TYPE_DIR)
					{
						//해당 path가 파일이 아님
						error_message("rm", path, -10);
						return;
					}
					else if (si_tmp.sfi_type == SFS_TYPE_FILE)
					{
						int k, bbnum_tmp;
						int sd_ino = sd[i].sfd_ino;
						int qt, rd; //비트맵에서 해당 비트를 찾기 위한 ino의 몫qt과 나머지rd
						struct sfs_btmp sb;

						nsd_tmp = sizeof(si_tmp.sfi_direct) / sizeof(u_int32_t);
						for (k = 0; k < nsd_tmp; k++)
						{
							if (si_tmp.sfi_direct[k] > 1 + bbnum)
							{
								bbnum_tmp = si_tmp.sfi_direct[k] / SFS_BLOCKBITS;
								bbnum_tmp += 2;

								disk_read(&sb, bbnum_tmp); //0:super 1:root 2~:bitmap
								qt = si_tmp.sfi_direct[k] / 8;
								rd = si_tmp.sfi_direct[k] % 8;
								BIT_CLEAR(sb.sfb_bytes[qt], rd);
								disk_write(&sb, bbnum_tmp);
							}
						}

						bbnum_tmp = sd[i].sfd_ino / SFS_BLOCKBITS;
						bbnum_tmp += 2;

						disk_read(&sb, bbnum_tmp); //0:super 1:root 2~:bitmap
						qt = sd[i].sfd_ino / 8;
						rd = sd[i].sfd_ino % 8;
						BIT_CLEAR(sb.sfb_bytes[qt], rd);

						for (k = i; k < nsd - 1; k++)
						{
							sd[k].sfd_ino = sd[k + 1].sfd_ino;
							strncpy(sd[k].sfd_name, sd[k + 1].sfd_name, SFS_NAMELEN); //(복사받는 문자열, 복사할 문자열)
						}
						sd[nsd - 1].sfd_ino = SFS_NOINO;
						sd[nsd - 1].sfd_name[0] = '\0';
						disk_write(sd, si.sfi_direct[0]);

						si_tmp.sfi_size = 0;			  //지우고자하는 파일의 크기 0으로
						si_tmp.sfi_type = SFS_TYPE_INVAL; //지우고자하는 파일타입 inval
						for (k = 0; k < SFS_NDIRECT; k++)
						{
							//파일의 모든 direct데이터블록 릴리즈
							si_tmp.sfi_direct[k] = 0;
						}
						si_tmp.sfi_indirect = 0; //파일의 indirect데이터블록 릴리즈
						disk_write(&si_tmp, sd_ino);

						si.sfi_size -= sizeof(struct sfs_dir); //디렉토리 엔트리의 inode크기 조정
						disk_write(&si, sd_cwd.sfd_ino);

						disk_write(&sb, bbnum_tmp);

						return;
					}
				}
			}
		}

		//block access
		disk_read(sd, si.sfi_direct[k]); //해당 inode의 블록을 sd로 가져옴. sd[0]은 현재 디렉토리,sd[1]은 부모 디렉토리

		for (i = 0; i < l; i++)
		{
			disk_read(&si_tmp, sd[i].sfd_ino);
			if (!strcmp(sd[i].sfd_name, path)) //원하는 디렉토리 찾음
			{
				if (si_tmp.sfi_type == SFS_TYPE_DIR)
				{
					//해당 path가 파일이 아님
					error_message("rm", path, -10);
					return;
				}
				else if (si_tmp.sfi_type == SFS_TYPE_FILE)
				{
					int k, bbnum_tmp;
					int sd_ino = sd[i].sfd_ino;
					int qt, rd; //비트맵에서 해당 비트를 찾기 위한 ino의 몫qt과 나머지rd
					struct sfs_btmp sb;

					nsd_tmp = sizeof(si_tmp.sfi_direct) / sizeof(u_int32_t);
					for (k = 0; k < nsd_tmp; k++)
					{
						if (si_tmp.sfi_direct[k] > 1 + bbnum)
						{
							bbnum_tmp = si_tmp.sfi_direct[k] / SFS_BLOCKBITS;
							bbnum_tmp += 2;

							disk_read(&sb, bbnum_tmp); //0:super 1:root 2~:bitmap
							qt = si_tmp.sfi_direct[k] / 8;
							rd = si_tmp.sfi_direct[k] % 8;
							BIT_CLEAR(sb.sfb_bytes[qt], rd);
							disk_write(&sb, bbnum_tmp);

							//spb.sp_nblocks--;
						}
					}

					bbnum_tmp = sd[i].sfd_ino / SFS_BLOCKBITS;
					bbnum_tmp += 2;

					disk_read(&sb, bbnum_tmp); //0:super 1:root 2~:bitmap
					qt = sd[i].sfd_ino / 8;
					rd = sd[i].sfd_ino % 8;
					BIT_CLEAR(sb.sfb_bytes[qt], rd);

					for (k = i; k < nsd - 1; k++)
					{
						sd[k].sfd_ino = sd[k + 1].sfd_ino;
						strncpy(sd[k].sfd_name, sd[k + 1].sfd_name, SFS_NAMELEN); //(복사받는 문자열, 복사할 문자열)
					}
					sd[nsd - 1].sfd_ino = SFS_NOINO;
					sd[nsd - 1].sfd_name[0] = '\0';
					disk_write(sd, si.sfi_direct[0]);

					si_tmp.sfi_size = 0;			  //지우고자하는 파일의 크기 0으로
					si_tmp.sfi_type = SFS_TYPE_INVAL; //지우고자하는 파일타입 inval
					for (k = 0; k < SFS_NDIRECT; k++)
					{
						//파일의 모든 direct데이터블록 릴리즈
						si_tmp.sfi_direct[k] = 0;
					}
					si_tmp.sfi_indirect = 0; //파일의 indirect데이터블록 릴리즈
					disk_write(&si_tmp, sd_ino);

					disk_write(&si, sd_cwd.sfd_ino);

					disk_write(&sb, bbnum_tmp);

					return;
				}
			}
			/*if(si_tmp.sfi_type == SFS_TYPE_DIR)
				{
					qarr[++idx]=sd[i].sfd_ino;//푸쉬. q.push(sd[i].sfd_ino);
				}*/
		}
	}
	//해당 파일이 존재하지 않음
	error_message("rm", path, -1);
}

void sfs_cpin(const char *local_path, const char *path)
{
	//printf("Not Implemented\n");

	struct sfs_inode si, si_btmp, newbie;							   //,si_tmp;
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd_tmp[SFS_DENTRYPERBLOCK]; //디렉토리 엔트리 배열
	struct sfs_btmp sb;
	int direct[SFS_DBPERIDB];
	int i, j, k, l, nsd, nsd_tmp, bbnum_tmp;
	int swch = 0, count_buf = 0, count_dir = 0, count_indir = 0, total_buf = 0;
	int newbie_ino = -1, ino_tmp = -1;
	FILE *fp;
	//direct block 1개당 512B 총15개.
	//indirect block 1개있고 512/4개의 엔트리(=direct block), 엔트리당 512B.
	char buf[SFS_BLOCKSIZE]; // 블록하나에 512B. dir<=7680B, indir<=64KB . 7680+64*1024

	disk_read(&si, sd_cwd.sfd_ino); //현재 디렉토리의 inode정보 read
	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR); //si가 디렉토리의 inode인지

	nsd = si.sfi_size / sizeof(struct sfs_dir); //nsd=si.sfi_size/sizeof(struct sfs_dir); //nsd은 해당 inode의 디렉토리 엔트리 수
	k = nsd / SFS_DENTRYPERBLOCK;				//k번째 다이렉트 사용, SFS_DENTRYPERBLOCK=8
	l = nsd % SFS_DENTRYPERBLOCK;				//k번째 다이렉트에 l개 엔트리 있음

	if (k >= SFS_NDIRECT)
	{
		//할당 받을 디렉토리 엔트리가 없는 경우
		error_message("cpin", local_path, -3);
		return;
	}

	if (l == 0)
	{
		si.sfi_direct[k] = -1;
		si.sfi_direct[k] = getBbnum();
		if (si.sfi_direct[k] == -1)
		{
			//가용 블락이 없는 경우 에러,종료
			error_message("cpin", local_path, -4);
			return;
		}
	}

	for (j = 0; j < k; j++)
	{
		//block access
		disk_read(sd, si.sfi_direct[j]); //현재 디렉토리의 디렉토리엔트리들 read

		for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
		{
			//disk_read( &si_tmp,sd[i].sfd_ino );
			if (!strcmp(sd[i].sfd_name, local_path)) //만들고자 하는 파일과 같은 이름 발견
			{
				//같은 이름이 있으므로 에러
				error_message("cpin", local_path, -6);
				return;
			}
		}
	}
	bzero(sd, SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir));
	//block access
	disk_read(sd, si.sfi_direct[k]); //현재 디렉토리의 디렉토리엔트리들 read
	for (i = 0; i < l; i++)
	{
		//disk_read( &si_tmp,sd[i].sfd_ino );
		if (!strcmp(sd[i].sfd_name, local_path)) //만들고자 하는 파일과 같은 이름 발견
		{
			//같은 이름이 있으므로 에러
			error_message("cpin", local_path, -6);
			return;
		}
	}
	bzero(sd, SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir));

	// path에 해당하는 파일의 크기 얻음 st
	if ((fp = fopen(path, "r")) == NULL)
	{
		//호스트의 파일 읽기 오류
		error_message("cpin", path, -11);
		return;
	}
	//가용 블록의 비트 구함 st
	newbie_ino = getBbnum();
	if (newbie_ino == -1)
	{
		//가용 블락이 없는 경우 에러,종료
		error_message("cpin", local_path, -4);
		return;
	}
	//가용 블록의 비트 구함 ed

	disk_read(sd, si.sfi_direct[k]);
	sd[l].sfd_ino = newbie_ino;
	strncpy(sd[l].sfd_name, local_path, SFS_NAMELEN); //(복사받는 문자열, 복사할 문자열)
	disk_write(sd, si.sfi_direct[k]);				  //수정된 sd를 si 디렉토리 엔트리에 write

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write(&si, sd_cwd.sfd_ino);

	bzero(&newbie, SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;
	//disk_write( &newbie, newbie_ino );

	while (feof(fp) == 0)
	{
		ino_tmp = -1;
		//한 블록크기 만큼씩 buf에 read함.
		//count_buf는 읽어들인 크기(바이트단위)
		count_buf = fread(buf, sizeof(char), SFS_BLOCKSIZE, fp);
		total_buf += count_buf;

		newbie.sfi_size += count_buf;

		if (count_dir < 15) //direct포인터 이용
		{
			ino_tmp = getBbnum();
			if (ino_tmp == -1)
			{
				//가용 블락이 없는 경우 에러,종료
				error_message("cpin", local_path, -4);
				disk_write(&newbie, newbie_ino);
				return;
			}
			newbie.sfi_direct[count_dir] = ino_tmp;
			disk_write(buf, ino_tmp);
			count_dir++;
		}
		else //indirect포인터 이용
		{
			if (count_indir == 0) //indirect 포인터 처음 사용
			{
				ino_tmp = getBbnum();
				if (ino_tmp == -1)
				{
					//가용 블락이 없는 경우 에러,종료
					error_message("cpin", local_path, -4);
					disk_write(&newbie, newbie_ino);
					return;
				}
				newbie.sfi_indirect = ino_tmp;
				bzero(direct, SFS_DBPERIDB);
				disk_write(direct, ino_tmp);
			}

			ino_tmp = -1;
			ino_tmp = getBbnum();
			if (ino_tmp == -1)
			{
				//가용 블락이 없는 경우 에러,종료
				error_message("cpin", local_path, -4);
				disk_write(&newbie, newbie_ino);
				return;
			}
			disk_read(direct, newbie.sfi_indirect);
			direct[count_indir] = ino_tmp;
			disk_write(direct, newbie.sfi_indirect);
			disk_write(buf, ino_tmp);
			count_indir++;
			if (count_indir >= SFS_DBPERIDB)
			{
				break;
			}
		}
		//bzero(sd, SFS_DENTRYPERBLOCK*sizeof(struct sfs_dir));
		bzero(buf, SFS_BLOCKSIZE);
	}
	disk_write(&newbie, newbie_ino);

	fclose(fp);
}

void sfs_cpout(const char *local_path, const char *path)
{
	//printf("Not Implemented\n");

	struct sfs_inode si, si_tmp, newbie;							   //,si_tmp;
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd_tmp[SFS_DENTRYPERBLOCK]; //디렉토리 엔트리 배열
	struct sfs_btmp sb;
	int direct[SFS_DBPERIDB];
	int i, j, k, l, nsd, nsd_tmp, bbnum_tmp;
	int swch = 0, count_buf = 0, count_dir = 0, count_indir = 0, total_buf = 0;
	int newbie_ino = -1, ino_tmp = -1, localpath_ino = -1, path_ino = -1;
	FILE *fp;
	//direct block 1개당 512B 총15개.
	//indirect block 1개있고 512/4개의 엔트리(=direct block), 엔트리당 512B.
	char buf[SFS_BLOCKSIZE]; // 블록하나에 512B. dir<=7680B, indir<=64KB . 7680+64*1024

	disk_read(&si, sd_cwd.sfd_ino); //현재 디렉토리의 inode정보 read
	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR); //si가 디렉토리의 inode인지

	nsd = si.sfi_size / sizeof(struct sfs_dir); //nsd=si.sfi_size/sizeof(struct sfs_dir); //nsd은 해당 inode의 디렉토리 엔트리 수
	k = nsd / SFS_DENTRYPERBLOCK;				//k번째 다이렉트 사용, SFS_DENTRYPERBLOCK=8
	l = nsd % SFS_DENTRYPERBLOCK;				//k번째 다이렉트에 l개 엔트리 있음

	//local_path에 해당하는 파일 탐색해서 해당 파일의 inode#알아내고 탈출 st
	for (j = 0; j < k; j++)
	{
		//block access
		disk_read(sd, si.sfi_direct[j]); //현재 디렉토리의 디렉토리엔트리들 read

		for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
		{
			//disk_read( &si_tmp,sd[i].sfd_ino );
			if (!strcmp(sd[i].sfd_name, local_path)) //만들고자 하는 파일과 같은 이름 발견
			{
				localpath_ino = sd[i].sfd_ino;
				swch = 1;
			}
		}
	}
	bzero(sd, SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir));
	//block access
	disk_read(sd, si.sfi_direct[k]); //현재 디렉토리의 디렉토리엔트리들 read
	for (i = 0; i < l; i++)
	{
		//disk_read( &si_tmp,sd[i].sfd_ino );
		if (!strcmp(sd[i].sfd_name, local_path)) //만들고자 하는 파일과 같은 이름 발견
		{
			localpath_ino = sd[i].sfd_ino;
			swch = 1;
		}
	}
	bzero(sd, SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir));
	//local_path에 해당하는 파일 탐색해서 해당 파일의 inode#알아내고 탈출 ed
	if (swch == 0)
	{
		//호스트의 파일 읽기 오류
		error_message("cpout", local_path, -1);
		return;
	}

	if ((fp = fopen(path, "w")) == NULL)
	{
		//호스트의 파일 읽기 오류
		error_message("cpout", path, -6);
		return;
	}
	if (fread(buf, sizeof(char), SFS_BLOCKSIZE, fp) != 0)
	{
		//이미 존재하는 호스트파일
		error_message("cpout", path, -6);
		return;
	}
	bzero(buf, SFS_BLOCKSIZE);

	disk_read(&si_tmp, localpath_ino);
	count_dir = si_tmp.sfi_size / SFS_BLOCKSIZE;
	if (count_dir >= 15) //count_dir,count_indir은 모두 0부터 시작
	{
		count_indir = count_dir - 14;
		count_dir = 14;
	}

	for (i = 0; i <= count_dir; i++)
	{
		disk_read(buf, si_tmp.sfi_direct[i]);
		fwrite(buf, sizeof(char), sizeof(buf), fp);
		bzero(buf, SFS_BLOCKSIZE);
	}
	disk_read(direct, si_tmp.sfi_indirect);
	for (i = 0; i <= count_indir; i++)
	{
		disk_read(buf, direct[i]);
		fwrite(buf, sizeof(char), sizeof(buf), fp);
		bzero(buf, SFS_BLOCKSIZE);
	}

	fclose(fp);
}

void dump_inode(struct sfs_inode inode)
{
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for (i = 0; i < SFS_NDIRECT; i++)
	{
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d", inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR)
	{
		for (i = 0; i < SFS_NDIRECT; i++)
		{
			if (inode.sfi_direct[i] == 0)
				break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}
}

void dump_directory(struct sfs_dir dir_entry[])
{
	int i;
	struct sfs_inode inode;
	for (i = 0; i < SFS_DENTRYPERBLOCK; i++)
	{
		printf("%d %s\n", dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode, dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE)
		{
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump()
{
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n", sd_cwd.sfd_ino, sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");
}
