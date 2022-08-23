#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
 
int main(int argc, char* argv[])
{
	if(strcmp(argv[1], "-c") == 0) {
		DIR *currDir = opendir(argv[2]), *currDir1 = opendir(argv[2]);
		struct dirent *Dirent = readdir(currDir), *Dirent1 = readdir(currDir1);
		int size = strlen(argv[2]) + strlen(argv[3] + 1);
		char tarfilepath[size];
		sprintf(tarfilepath, "%s/%s", argv[2], argv[3]);
		int tarfd = open(tarfilepath, O_WRONLY|O_CREAT, 0644);
		if(tarfd < 0) {
			perror("Failed to complete creation operation\n");
			exit(-1);
		}
		int numfiles = 0;
		while(Dirent1 != NULL) {
			if(strcmp(Dirent1->d_name, "..") == 0 || strcmp(Dirent1->d_name, ".") == 0) {
				Dirent1 = readdir(currDir1);
				continue;
			}
			numfiles++;
			Dirent1 = readdir(currDir1);
		}
		closedir(currDir1);
		char numfile[20];
		int len;
		len = sprintf(numfile, "%d\n", numfiles);
		write(tarfd, numfile, len);
		while(Dirent != NULL) {
			if(strcmp(Dirent->d_name, "..") == 0 || strcmp(Dirent->d_name, ".") == 0) {
				Dirent = readdir(currDir);
				continue;
			}
			char filename[strlen(argv[2])+ 20];
			len = sprintf(filename,"%s/%s", argv[2], Dirent->d_name);
			assert(write(tarfd, Dirent->d_name, strlen(Dirent->d_name)) == strlen(Dirent->d_name));
			assert(write(tarfd, "\n", 1) == 1);
			int fd = open(filename, O_RDONLY);	
			if(fd < 0){
       			perror("Failed to complete creation operation\n");
       			exit(-1);
  			}
			long int filesize = lseek(fd, 0, SEEK_END);
			char Filesize[20];
			len = sprintf(Filesize, "%ld\n", filesize);
			assert(write(tarfd, Filesize, len) == len);
			lseek(fd, 0, SEEK_SET);	
			char buf[filesize];
			assert(read(fd, buf, filesize) == filesize);
			assert(write(tarfd, buf, filesize) == filesize);
			close(fd);
			Dirent = readdir(currDir);
		}
		close(tarfd);
		closedir(currDir);
	}
	else if(strcmp(argv[1], "-d") == 0) {
		int len = strlen(argv[2]);
		char directorypath[len + 1];
		sprintf(directorypath, "%s", argv[2]);
		directorypath[len - 4] = 'D';
		directorypath[len - 3] = 'u';
		directorypath[len - 2] = 'm';
		directorypath[len - 1] = 'p';
		directorypath[len] = '\0';
		mkdir(directorypath, 0777);
		int numfiles = 0;
		int tarfd = open(argv[2], O_RDONLY);
		if(tarfd < 0) {
			perror("Failed to complete extraction operation\n");
			exit(-1);
		}
		char buf1[1];
		while(1) {
			assert(read(tarfd, buf1, 1) == 1);
			if(buf1[0] == '\n') break;
			numfiles = 10*numfiles + buf1[0] - '0';
		}
		while(numfiles--) {
			char filename[20];
			int pos = 0;
			while(1) {
				assert(read(tarfd, buf1, 1) == 1);
				if(buf1[0] == '\n') break;
				filename[pos] = buf1[0];
				pos++;
			}
			filename[pos] = '\0';
			char absolutepathfile[100];
			len = sprintf(absolutepathfile, "%s/%s", directorypath, filename);
			int fd = open(absolutepathfile, O_WRONLY|O_CREAT, 0644);
			if(fd < 0){
       			perror("Failed to complete extraction operation\n");
       			exit(-1);
  			}
			int filesize = 0;
			while(1) {
				assert(read(tarfd, buf1, 1) == 1);
				if(buf1[0] == '\n') break;
				filesize = 10*filesize + buf1[0] - '0';
			}
			char buf[filesize];
			assert(read(tarfd, buf, filesize) == filesize);
			assert(write(fd, buf, filesize) == filesize);
			close(fd);
		}
		close(tarfd);
	}
	else if(strcmp(argv[1], "-e") == 0) {
		char directorypath[100];
		int lastslash = 0, pos = 0;
		for(int i = 0; i < strlen(argv[2]); i++)
			if(argv[2][i] == '/') lastslash = i;
		for(int i = 0; i < 100; i++) {
			directorypath[i] = argv[2][i];
			if(i == lastslash) break;
			pos++;
		}
		char directoryname[] = "IndividualDump";
		for(int i = 0; i < strlen(directoryname); i++) {
			pos++;
			directorypath[pos] = directoryname[i];
		}
		directorypath[pos + 1] = '\0';
		mkdir(directorypath, 0777);
		int filefound = 0;
		int numfiles = 0;
		int tarfd = open(argv[2], O_RDONLY);
		if(tarfd < 0) {
			perror("Failed to complete extraction operation\n");
			exit(-1);
		}
		char buf1[1];
		while(1) {
			assert(read(tarfd, buf1, 1) == 1);
			if(buf1[0] == '\n') break;
			numfiles = 10*numfiles + buf1[0] - '0';
		}
		while(numfiles--) {
			char filename[20];
			int pos = 0;
			while(1) {
				assert(read(tarfd, buf1, 1) == 1);
				if(buf1[0] == '\n') break;
				filename[pos] = buf1[0];
				pos++;
			}
			filename[pos] = '\0';
			char absolutepathfile[200];
			int len = sprintf(absolutepathfile, "%s/%s", directorypath, filename);
			int filesize = 0;
			while(1) {
				assert(read(tarfd, buf1, 1) == 1);
				if(buf1[0] == '\n') break;
				filesize = 10*filesize + buf1[0] - '0';
			}
			char buf[filesize];
			assert(read(tarfd, buf, filesize));
			if(strcmp(filename, argv[3]) == 0) {
				filefound = 1;
				int fd = open(absolutepathfile, O_WRONLY|O_CREAT, 0644);
				if(fd < 0){
	       			perror("Failed to complete extraction operation\n");
	       			exit(-1);
	  			}
				assert(write(fd, buf, filesize) == filesize);
				close(fd);
				break;
			}
		}
		if(filefound == 0) printf("No such file is present in tar file.\n");
		close(tarfd);
	}
	else if(strcmp(argv[1], "-l") == 0) {
		char filepath[100];
		int lastslash = 0, pos = 0;
		for(int i = 0; i < strlen(argv[2]); i++)
			if(argv[2][i] == '/') lastslash = i;
		for(int i = 0; i < 100; i++) {
			filepath[i] = argv[2][i];
			if(i == lastslash) break;
			pos++;
		}
		char filename[] = "tarStructure";
		for(int i = 0; i < strlen(filename); i++) {
			pos++;
			filepath[pos] = filename[i];
		}
		filepath[pos + 1] = '\0';
		int fd = open(filepath, O_WRONLY|O_CREAT, 0644);
		if(fd < 0){
	       	perror("Failed to complete extraction operation\n");
	       	exit(-1);
	  	}
		int tarfd = open(argv[2], O_RDONLY);
		if(tarfd < 0) {
			perror("Failed to complete list operation\n");
			exit(-1);
		}
		struct stat statbuf;
		fstat(tarfd, &statbuf);
		char fsize[25];
		int len = sprintf(fsize, "%ld\n", statbuf.st_size);
		assert(write(fd, fsize, len) == len);
		char buf1[1];
		int numfiles = 0;
		while(1) {
			assert(read(tarfd, buf1, 1) == 1);
			if(buf1[0] == '\n') break;
			numfiles = 10*numfiles + buf1[0] - '0';
		}
		len = sprintf(fsize, "%d\n", numfiles);
		assert(write(fd, fsize, len) == len);
		while(numfiles--) {
			char filename[20];
			int pos = 0;
			while(1) {
				assert(read(tarfd, buf1, 1) == 1);
				if(buf1[0] == '\n') break;
				filename[pos] = buf1[0];
				pos++;
			}
			filename[pos] = '\0';
			assert(write(fd, filename, pos) == pos);
			assert(write(fd, " ", 1) == 1);
			int filesize = 0;
			while(1) {
				assert(read(tarfd, buf1, 1) == 1);
				if(buf1[0] == '\n') break;
				filesize = 10*filesize + buf1[0] - '0';
			}
			len = sprintf(fsize, "%d\n", filesize);
			assert(write(fd, fsize, len) == len);
			char buf[filesize];
			assert(read(tarfd, buf, filesize));
		}
		close(tarfd);
	}
	return 0;
}
