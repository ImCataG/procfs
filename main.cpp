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

int elem = 0; // number of processes

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
} procs[10001]; // list of processes and details

std::unordered_map<int, int> pid_to_index; // holds the index where a process with a specified pid is in the procs list


std::vector<int> dirpath; // while doing dfs, dirpath is the path to a process directory, separated into vector elements
std::string path; // while doing dfs, path holds the path to a process directory, as a string

// FILE *gp; // file used for testing purposes

void dfs(int depth, int curr){ // normal dfs where we assign the proper path to every single procees
    procs[curr].visited = true;
    auto prevpath = path;
    procs[curr].dirpath = dirpath;
    dirpath.push_back(curr);

    // fprintf(gp, "%d %s%s\n", procs[curr].pid, procs[curr].comm, path.c_str());
    
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
    // creates or overwrites file output.txt with data about all processes currently running
    // uses grep and awk to format that data

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

    while ((read = getline(&line, &len, fp)) != -1) { // read until end of file
        lineno += 1;
        char current[100];
        // printf("Retrieved line of length %zu: ", read);
        // printf("\n%s", line);
        if(lineno == 1 || lineno == 2){
            continue;
        }
        if (lineno % 2 == 1){ // odd lines contain details about a process
            char *token = strtok(line, ":"); // data is separed by ':' in the file
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
		        // creating edge from parent id node to child node in a directed graph, similar to --forest option in ps
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
        else{ // even lines contain the command name of the process
            strcpy(procs[elem++].comm, line + 5);
        }
    }
    printf("%d processes.\n", elem);
}

int do_getattr (const char *path, struct stat *stbuf){ // FUSE getattr function
    if(strcmp(path, "/") == 0){ // the root directory, marked as a folder
        stbuf->st_mode = S_IFDIR | 0755;
        return 0;
    } 

    if(strcmp(path, "/proc") == 0){ // same for the proc directory
        stbuf->st_mode = S_IFDIR | 0755;
        return 0;
    } 

    for(int i = 0; i < elem; i ++){ // all folders marked as folders
        char current[100] = "/proc";
        strcat(current, procs[i].fullpath.c_str());
        if(strcmp(path, current) == 0){
            stbuf->st_mode = S_IFDIR | 0755;
            return 0;
        } 

        char current2[100] = "/proc"; // all stats files marked as files
        strcat(current2, procs[i].fullpath.c_str());
        strcat(current2, "/stats");
        if(strcmp(path, current2) == 0){
            stbuf->st_mode = S_IFREG | 0644;
            stbuf->st_size = 1000;
            return 0;
        } 
    }


    return 0;
}

int do_readdir(const char *path, void *buf, fuse_fill_dir_t handler, off_t, struct fuse_file_info *fi){ // FUSE readdir function

    static struct stat regular_file = {.st_mode = S_IFREG | 0644}; 
    static struct stat regular_directory = {.st_mode = S_IFDIR | 0755};

    if(strcmp(path, "/") == 0){ // creates the proc folder
        handler(buf, "proc", &regular_directory, 0);
    } 

    for(int i = 0; i < elem; i ++){ // creates a folder for each process. since all parents come before their children in the process list,
        char current[100] = "/proc"; // it will always work
        strcat(current, procs[i].parentpath.c_str());
        if(strcmp(path, current) == 0){
            char pid[100];
            snprintf(pid, 100, "%d", procs[i].pid);
            handler(buf, pid, &regular_directory, 0);
        }
    }

    for(int i = 0; i < elem; i ++){ // creates the stats file inside each folder
        char current[100] = "/proc";
        strcat(current, procs[i].fullpath.c_str());
        if(strcmp(path, current) == 0){
            handler(buf, "stats", &regular_file, 0);
        }
    }
    
    return 0;
}

int do_access(const char *path, int mode) { // idk lmao
    // printf("!!! ACCESS !!! %s\n", path);
    return 0;
}

int do_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){ // FUSE read function
    // printf("!!! READ !!! %s\n", path);

    for(int i = 0; i < elem; i ++){ // makes sure all stats files are filled with correct information
        char current[100] = "/proc";
        strcat(current, procs[i].fullpath.c_str());
        strcat(current, "/stats");
        if(strcmp(path, current) == 0){
            char stats[1000];
            snprintf(stats, 1000, "pid: %d\nppid: %d\nni: %s\npcpu: %s\npmem: %s\ncomm: %s\n", procs[i].pid, procs[i].ppid, procs[i].ni, procs[i].pcpu, procs[i].pmem, procs[i].comm);
            memcpy(buf, stats, strlen(stats));
            return strlen(stats); // returns length of data that can be read from the file
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
    // gp = fopen("foldertest.txt", "w");

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

    // printf("dfsd!\n\n");

    // fclose(gp);

    return fuse_main(argc, argv, &do_oper, NULL);
}


/*
g++ main.cpp -o procfs -g `pkg-config --libs --cflags fuse` --std=c++17 -pthread
*/