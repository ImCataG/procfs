#define FUSE_USE_VERSION 26
#include <stdio.h>
#include "oldfuse/fuse.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <vector>
#include <unordered_map>
#include <string>

typedef int (*fuse_fill_dir_t) (void *buf, const char *name, const struct stat *stbuf, off_t off);

int elem = 0;

struct proc{
    int pid;
    int ppid;
    char ni[100];
    char pcpu[100];
    char pmem[100];
    char comm[100];
    bool visited = false;
    std::vector<int> adjlist;
    std::vector<int> dirpath;
    std::string fullpath;
    std::string parentpath;
} procs[10001]; // lista cu toate procesele si detalii

std::unordered_map<int, int> pid_to_index; // pentru a afla unde se afla in lista procs procesul cu un anume PID

std::string path;
std::vector<int> dirpath;

void dfs(int depth, int curr){ 
    procs[curr].visited = true;
    auto prevpath = path;
    procs[curr].dirpath = dirpath;
    dirpath.push_back(curr);

    // open file foldertext.txt for write

    FILE *gp;
    gp = fopen("foldertest.txt", "a");
    fprintf(gp, "%d %s%s\n", procs[curr].pid, procs[curr].comm, path.c_str());
    fclose(gp);

    // g << procs[curr].pid << " " << procs[curr].comm << path << "\n";
    procs[curr].parentpath = path;
    path += "/";
    path += std::__cxx11::to_string(procs[curr].pid);
    procs[curr].fullpath = path;
    // printf("%s %d\n", procs[curr].comm, procs[curr].pid);
    for(int i = 0; i < procs[curr].adjlist.size(); i ++){
        if(!procs[procs[curr].adjlist[i]].visited)
            dfs(depth + 1, procs[curr].adjlist[i]);
    }
    path = prevpath;
    dirpath.pop_back();


}

void get_processes(){
    char command[] = "ps -eo pid,ppid,ni,pcpu,pmem,comm:100 -H | grep -v grep | awk {'print $1\":\"$2\":\"$3\":\"$4\":\"$5\":\"; $1=\"\";$2=\"\";$3=\"\";$4=\"\";$5=\"\";print'} > output.txt";
    // the actual command: ps -eo pid,ppid,ni,pcpu,pmem,comm:100 -H | grep -v grep | awk {'print $1":"$2":"$3":"$4":"$5":"; $1="";$2="";$3="";$4="";$5="";print'} > output.txt
    //execve(app, args, NULL);

    system(command);
}

void parse(){
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int lineno = 0;

    fp = fopen("output.txt", "r");
    
    if (fp == NULL)
        exit(EXIT_FAILURE);

    while ((read = getline(&line, &len, fp)) != -1) {
        lineno += 1;
        char current[100];
        // printf("Retrieved line of length %zu: ", read);
        // printf("\n%s", line);
        if(lineno == 1 || lineno == 2){
            continue;
        }
        if (lineno % 2 == 1){
            char *token = strtok(line, ":"); // separ in functie de ":", in formatul din output.txt
            int i = 0;
            while(token != NULL){
                // printf("%s\n", token);
                if(i == 0){
                    procs[elem].pid = atoi(token);
                    pid_to_index[atoi(token)] = elem;
                }
                if(i == 1 && atoi(token) != 0){
                    procs[elem].ppid = atoi(token);
                    procs[pid_to_index[atoi(token)]].adjlist.push_back(elem);
		    // duc muchie de la parinte la copil pentru a face o reprezentare similara cu forest-ul din ps
                }
                if(i == 2){
                    strcpy(procs[elem].ni, token);
                }
                if(i == 3){
                    strcpy(procs[elem].pcpu, token);
                }
                if(i == 4){
                    strcpy(procs[elem].pmem, token);
                }
                i += 1;
                token = strtok(NULL, ":");
            }
        }
        else{
            strcpy(procs[elem++].comm, line + 5);
        }
    }
    printf("%d processes.\n", elem);
}

int do_getattr (const char *path, struct stat *stbuf){
    if(strcmp(path, "/") == 0){
        stbuf->st_mode = S_IFDIR | 0755;
    }

    if(strcmp(path, "/proc") == 0){
        stbuf->st_mode = S_IFDIR | 0755;
    }

    for(int i = 0; i < elem; i ++){
        char current[100] = "/proc";
        strcat(current, procs[i].fullpath.c_str());
        if(strcmp(path, current) == 0){
            stbuf->st_mode = S_IFDIR | 0755;
        }

        char current2[100] = "/proc";
        strcat(current2, procs[i].fullpath.c_str());
        strcat(current2, "/stats");
        if(strcmp(path, current2) == 0){
            stbuf->st_mode = S_IFREG | 0644;
            stbuf->st_size = 1000;
        }
    }


    return 0;
}

int do_readdir(const char *path, void *buf, fuse_fill_dir_t handler, off_t, struct fuse_file_info *fi){

    static struct stat regular_file = {.st_mode = S_IFREG | 0644};
    static struct stat regular_directory = {.st_mode = S_IFDIR | 0755};
    if(strcmp(path, "/") == 0){
        handler(buf, "proc", &regular_directory, 0);
    }

    for(int i = 0; i < elem; i ++){
        char current[100] = "/proc";
        strcat(current, procs[i].parentpath.c_str());
        if(strcmp(path, current) == 0){
            char pid[100];
            snprintf(pid, 100, "%d", procs[i].pid);
            handler(buf, pid, &regular_directory, 0);
        }
    }

    for(int i = 0; i < elem; i ++){
        char current[100] = "/proc";
        strcat(current, procs[i].fullpath.c_str());
        if(strcmp(path, current) == 0){
            handler(buf, "stats", &regular_file, 0);
        }
    }
    
    return 0;
}

int do_access(const char *path, int mode) {
    printf("!!! ACCESS !!! %s\n", path);
    return 0;
}

int do_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    printf("!!! READ !!! %s\n", path);

    for(int i = 0; i < elem; i ++){
        char current[100] = "/proc";
        strcat(current, procs[i].fullpath.c_str());
        strcat(current, "/stats");
        if(strcmp(path, current) == 0){
            char stats[1000];
            snprintf(stats, 1000, "pid: %d\nppid: %d\nni: %s\npcpu: %s\npmem: %s\ncomm: %s\n", procs[i].pid, procs[i].ppid, procs[i].ni, procs[i].pcpu, procs[i].pmem, procs[i].comm);
            memcpy(buf, stats, strlen(stats));
            return strlen(stats);
        }
    }
    
    return 0;
}

static struct fuse_operations do_oper = {
    .getattr = do_getattr,
    .read = do_read,
    .readdir = do_readdir,
    .access = do_access,
};


int main(int argc, char * argv[]){
    printf("starting!\n");
    // clear file foldertest.txt
    FILE *fp = fopen("foldertest.txt", "w");
    fclose(fp);

    get_processes();
    printf("got em!\n");
    parse();
    printf("parsed em!\n");
    for(int i = 0; i < elem; i ++){
        path = "";
        dirpath.clear();
        if(!procs[i].visited)
            dfs(0, i);
    }
    printf("dfsd! check foldertest.txt\n\n");
    return fuse_main(argc, argv, &do_oper, NULL);
}


/*
g++ main.cpp -o procfs -g `pkg-config --libs --cflags fuse` --std=c++17 -pthread
*/