#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <stdint.h>
#include <libgen.h>

static int err_code;

static int BUF_SIZE = 1024;
static int BUFF_SIZE = 2048;
char cwd[1024]; 
char * dir_main = NULL; 
char dir_rec[2048];
int globl_argc;

/*
 * here are some function signatures and macros that may be helpful.
 */

void handle_error(char* fullname, char* action);
bool test_file(char* pathandname);
bool is_dir(char* pathandname);
const char* ftype_to_str(mode_t mode);
void list_file(char* pathandname, char* name, bool list_long);
void list_dir(char* dirname, bool list_long, bool list_all, bool recursive);

/*
 * You can use the NOT_YET_IMPLEMENTED macro to error out when you reach parts
 * of the code you have not yet finished implementing.
 */
#define NOT_YET_IMPLEMENTED(msg)                  \
    do {                                          \
        printf("Not yet implemented: " msg "\n"); \
        exit(255);                                \
    } while (0)

/*
 * PRINT_ERROR: This can be used to print the cause of an error returned by a
 * system call. It can help with debugging and reporting error causes to
 * the user. Example usage:
 *     if ( error_condition ) {
 *        PRINT_ERROR();
 *     }
 */
#define PRINT_ERROR(progname, what_happened, pathandname)               \
    do {                                                                \
        printf("%s: %s %s: %s\n", progname, what_happened, pathandname, \
               strerror(errno));                                        \
    } while (0)

/* PRINT_PERM_CHAR:
 *
 * This will be useful for -l permission printing.  It prints the given
 * 'ch' if the permission exists, or "-" otherwise.
 * Example usage:
 *     PRINT_PERM_CHAR(sb.st_mode, S_IRUSR, "r");
 */
#define PRINT_PERM_CHAR(mode, mask, ch) printf("%s", (mode & mask) ? ch : "-");

/*
 * Get username for uid. Return 1 on failure, 0 otherwise.
 */
static int uname_for_uid(uid_t uid, char* buf, size_t buflen) {
    struct passwd* p = getpwuid(uid);
    if (p == NULL) {
        return 1;
    }
    strncpy(buf, p->pw_name, buflen);
    return 0;
}

/*
 * Get group name for gid. Return 1 on failure, 0 otherwise.
 */
static int group_for_gid(gid_t gid, char* buf, size_t buflen) {
    struct group* g = getgrgid(gid);
    if (g == NULL) {
        return 1;
    }
    strncpy(buf, g->gr_name, buflen);
    return 0;
}

/*
 * Format the supplied `struct timespec` in `ts` (e.g., from `stat.st_mtim`) as a
 * string in `char *out`. Returns the length of the formatted string (see, `man
 * 3 strftime`).
 */
static size_t date_string(struct timespec* ts, char* out, size_t len) {
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    struct tm* t = localtime(&ts->tv_sec);
    if (now.tv_sec < ts->tv_sec) {
        // Future time, treat with care.
        return strftime(out, len, "%b %e %Y", t);
    } else {
        time_t difference = now.tv_sec - ts->tv_sec;
        if (difference < 31556952ull) {
            return strftime(out, len, "%b %e %H:%M", t);
        } else {
            return strftime(out, len, "%b %e %Y", t);
        }
    }
}

/*
 * Print help message and exit.
 */
static void help() {
    /* TODO: add to this */
    printf("ls: List files\n");
    printf("\t--help: Print this help\n");
    printf("\t-a: do not ignore entries starting with .\n");
    printf("\t-l: use a long listing format\n");
    printf("\t-R: list subdirectories recursively\n");
    exit(0);
}

/*
 * call this when there's been an error.
 * The function should:
 * - print a suitable error message (this is already implemented)
 * - set appropriate bits in err_code
 */
void handle_error(char* what_happened, char* fullname) {
    PRINT_ERROR("ls", what_happened, fullname);

    // TODO: your code here: inspect errno and set err_code accordingly.

    if(ENOENT == 2)
    {//a file specified in the command-line was not found
        err_code = err_code | (1 << 6) | (1 << 3);
    }

    if(EACCES == 13)
    {//program denied access to a file
        err_code = err_code | (1 << 6) | (1 << 4);
    }

    return;
}

/*
 * test_file():
 * test whether stat() returns successfully and if not, handle error.
 * Use this to test for whether a file or dir exists
 */
bool test_file(char* pathandname) {
    struct stat sb;
    if (stat(pathandname, &sb)) {
        handle_error("cannot access", pathandname);
        return false;
    }
    return true;
}

/*
 * is_dir(): tests whether the argument refers to a directory.
 * precondition: test_file() returns true. that is, call this function
 * only if test_file(pathandname) returned true.
 */
bool is_dir(char* pathandname) {
    /* TODO: fillin */
    struct stat status;
    if(stat(pathandname, &status) == -1) {
        return false;
    }
    return S_ISDIR(status.st_mode);
}

/* convert the mode field in a struct stat to a file type, for -l printing */
const char* ftype_to_str(mode_t mode) {
    /* TODO: fillin */
    if(S_ISREG(mode))
    {
        printf("-");
    }
    else if(S_ISDIR(mode))
    {
        printf("d");
    }
    else
    {
        printf("?");
    }

    //for user
    printf( (S_IRUSR & mode) ? "r" : "-"); //second character
    printf( (S_IWUSR & mode) ? "w" : "-"); //third character
    printf( (S_IXUSR & mode) ? "x" : "-"); //4th character

    //for group
    printf( (S_IRGRP & mode) ? "r" : "-"); //5th character
    printf( (S_IWGRP & mode) ? "w" : "-"); //6th character 
    printf( (S_IXGRP & mode) ? "x" : "-"); //7th character

    //for others
    printf( (S_IROTH & mode) ? "r" : "-"); //8th character
    printf( (S_IWOTH & mode) ? "w" : "-"); //9th character
    printf( (S_IXOTH & mode) ? "x" : "-"); //10th character

    return NULL;
}

/* list_file():
 * implement the logic for listing a single file.
 * This function takes:
 *   - pathandname: the directory name plus the file name.
 *   - name: just the name "component".
 *   - list_long: a flag indicated whether the printout should be in
 *   long mode.
 *
 *   The reason for this signature is convenience: some of the file-outputting
 *   logic requires the full pathandname (specifically, testing for a directory
 *   so you can print a '/' and outputting in long mode), and some of it
 *   requires only the 'name' part. So we pass in both. An alternative
 *   implementation would pass in pathandname and parse out 'name'.
 */
void list_file(char* pathandname, char* name, bool list_long) {
    /* TODO: fill in*/
    if(list_long == false){
        printf("%s\n", name);
    }
    else
    {
        struct stat status;
        if(stat(pathandname, &status) == -1) {
            return;
        }
        else
        {
            char uid_buf[BUF_SIZE];
            int uname = uname_for_uid(status.st_uid, uid_buf, BUF_SIZE);

            char gid_buf[BUF_SIZE];
            int group = group_for_gid(status.st_gid, gid_buf, BUF_SIZE);

            char time_buf[BUF_SIZE];
            date_string((struct timespec *) &status.st_mtime, time_buf, BUF_SIZE);

            ftype_to_str(status.st_mode);

            if(uname || group)
            {//print numeric UID or GID
                if(uname && group)
                {
                    printf(" %lu %d %d %6ld %s %s",  
                    status.st_nlink,
                    status.st_uid,
                    status.st_gid,
                    status.st_size,
                    time_buf,
                    name);
                }
                else if(uname)
                {
                    printf(" %lu %d %s %6ld %s %s",  
                    status.st_nlink,
                    status.st_uid,
                    gid_buf,
                    status.st_size,
                    time_buf,
                    name);
                }
                else if(group)
                {
                    printf(" %lu %s %d %6ld %s %s",  
                    status.st_nlink,
                    uid_buf,
                    status.st_gid,
                    status.st_size,
                    time_buf,
                    name);
                }
                err_code = err_code | (1 << 6)  | (1 << 5);
                if(is_dir(pathandname) && name[0] != '.')
                {
                    printf("/\n");
                }
                else
                {
                    printf("\n");
                }
                return;
            }

            printf(" %lu %s %s %6ld %s %s",  
            status.st_nlink,
            uid_buf,
            gid_buf,
            status.st_size,
            time_buf,
            name);

        }
        if( is_dir(pathandname)
        && strcmp(name, ".") != 0 
        && strcmp(name, "..") != 0
        && list_long)
        {// .hidden/
            printf("/\n");
        }
        else if(is_dir(pathandname) && name[0] != '.')
        {
            printf("/\n");
        }
        else
        {
            printf("\n");
        }
    }
}

/* list_dir():
 * implement the logic for listing a directory.
 * This function takes:
 *    - dirname: the name of the directory
 *    - list_long: should the directory be listed in long mode?
 *    - list_all: are we in "-a" mode?
 *    - recursive: are we supposed to list sub-directories?
 */
void list_dir(char* dirname, bool list_long, bool list_all, bool recursive) {
    /* TODO: fill in
     *   You'll probably want to make use of:
     *       opendir()
     *       readdir()
     *       list_file()
     *       snprintf() [to make the 'pathandname' argument to
     *          list_file(). that requires concatenating 'dirname' and
     *          the 'd_name' portion of the dirents]
     *       closedir()
     *   See the lab description for further hints
     */

    DIR * p_dir = opendir(dirname);
    struct dirent * p_dirent;
    if(p_dir)
    {
        while((p_dirent = readdir(p_dir)) != NULL)
        {
            char path[BUFF_SIZE];
            snprintf(path, BUFF_SIZE, "%s/%s", dirname, p_dirent -> d_name);
            
            test_file(path);
            if(is_dir(path))
            {
                if(list_all)
                {
                    if(!list_long)
                    {
                        if(strcmp(p_dirent -> d_name, ".") == 0 || strcmp(p_dirent -> d_name, "..") == 0)
                        {
                            printf("%s\n", p_dirent -> d_name); //no slash for "." and ".."
                        }
                        else
                        {
                            printf("%s/\n", p_dirent -> d_name);
                        }
                    }
                }
                else if(!list_all)
                {
                    if((p_dirent -> d_name)[0] == '.')
                    {
                        continue;
                    }
                    else
                    {
                        if(!list_long)
                        {
                            printf("%s/\n", p_dirent -> d_name);
                        }
                    }
                }
            }
            if(is_dir(path) && list_long)
            {
                if(!list_all && (p_dirent -> d_name)[0] != '.')
                {
                    list_file(path, p_dirent -> d_name, list_long);
                }
                else if(list_all)
                {
                    list_file(path, p_dirent -> d_name, list_long);
                }
            }

            if(!is_dir(path))
            {
                if(!list_all)
                {
                    if((p_dirent -> d_name)[0] != '.')
                    {
                        list_file(path, p_dirent -> d_name, list_long);
                    }
                }
                else
                {
                    list_file(path, p_dirent -> d_name, list_long);
                }  
            }
        }
    }
    if(p_dir == NULL)
    {
        err_code = err_code | (1 << 6) | (1 << 3);
    }
    else
    {
        closedir(p_dir);
    }    

    if(recursive)
    {
        DIR * p_dir2 = opendir(dirname);
        struct dirent * p_dirent2;
        if(p_dir2)
        {
            while((p_dirent2 = readdir(p_dir2)) != NULL)
            {
                recursive = true;
                if(!is_dir(dirname))
                {
                    recursive = false;
                    continue;
                }

                char path[BUFF_SIZE];
                snprintf(path, BUFF_SIZE, "%s/%s", dirname, p_dirent2 -> d_name);

                if(recursive)
                {
                    if((!list_all 
                    && is_dir(path) 
                    && (p_dirent2 -> d_name)[0] != '.')
                    || (list_all && is_dir(path)) )
                    {//is a directory
                    
                        // if(list_all && (p_dirent2 -> d_name)[0] == '.')
                        // {
                        //     recursive = false;
                        //     if(strcmp(p_dirent2 -> d_name, ".") != 0
                        //     && strcmp(p_dirent2 -> d_name, "..") != 0)
                        //     {//print hidden dir
                        //         printf("%s/", p_dirent2 -> d_name);
                        //     }
                        //     continue;
                        // }

                        if(list_all && (p_dirent2 -> d_name)[0] == '.')
                        {
                            recursive = false;
                            if(strcmp(p_dirent2 -> d_name, ".") != 0
                            && strcmp(p_dirent2 -> d_name, "..") != 0)
                            {
                                //print hidden dir
                            }
                            else
                            {
                                continue;
                            }
                        }

                        char main_buf[BUF_SIZE];
                        strcpy(main_buf, dir_rec);
                        char * ret = strstr(path, main_buf);

                        //remove current dir substring from ret string:
	                    int len = strlen(main_buf);
                        if (len > 0) 
                        {
                            char *p = ret;
                            while ((p = strstr(p, main_buf)) != NULL) 
                            {
                                memmove(p, p + len, strlen(p + len) + 1);
                            }
                        }

                        if(ret && (globl_argc == 2)) // ./ls -R without additional args
                        {
                            printf("\n.%s:\n", ret);
                        }
                        else if(ret)
                        {
                            ret = ret + 1;
                            printf("\n%s:\n", ret);
                        }

                        char path2[BUFF_SIZE];
                        snprintf(path2, BUFF_SIZE, "%s/%s", dirname, p_dirent2 -> d_name);
                        list_dir(path2, list_long, list_all, recursive); //recursion            
                    }
                }
            }
        }
        closedir(p_dir2);
    }
}





int main(int argc, char* argv[]) {
    // This needs to be int since C does not specify whether char is signed or
    // unsigned.
    int opt;
    err_code = 0;
    bool list_long = false, list_all = false, recursive = false;
    globl_argc = argc;
    // We make use of getopt_long for argument parsing, and this
    // (single-element) array is used as input to that function. The `struct
    // option` helps us parse arguments of the form `--FOO`. Refer to `man 3
    // getopt_long` for more information.
    struct option opts[] = {
        {.name = "help", .has_arg = 0, .flag = NULL, .val = '\a'}};

    // This loop is used for argument parsing. Refer to `man 3 getopt_long` to
    // better understand what is going on here.
    while ((opt = getopt_long(argc, argv, "1alR", opts, NULL)) != -1) {
        switch (opt) {
            case '\a':
                // Handle the case that the user passed in `--help`. (In the
                // long argument array above, we used '\a' to indicate this
                // case.)
                help();
                break;
            case '1':
                // Safe to ignore since this is default behavior for our version
                // of ls.
                break;
            case 'a':
                list_all = true;
                break;
                // TODO: you will need to add items here to handle the
                // cases that the user enters "-l" or "-R"
            case 'l':
                list_long = true;
                break;
            case 'R':
                recursive = true;
                break;
            default:
                printf("Unimplemented flag %d\n", opt);
                break;
        }
    }

    // TODO: Replace this.
    // if (optind < argc) {
    //     printf("Optional arguments: ");
    // }
    // for (int i = optind; i < argc; i++) {
    //     printf("%s ", argv[i]);
    // }
    // if (optind < argc) {
    //     printf("\n");
    // }

    //* PASS FILE NAME TO LIST_FILE
  
   ////char cwd[BUF_SIZE]; 

   if (getcwd(cwd, sizeof(cwd)) != NULL) {
        //Current working directory
   } 
   else
   {
       perror("getcwd() error\n");
       return 1;
   }


    
    if(optind == argc && !recursive)
    {//$ ./ls no arguments
        list_dir(cwd, list_long, list_all, recursive);
    }
    else if(!recursive)
    {//$ ./ls <dir1> <dir2> ...

        char path[BUFF_SIZE]; 
        struct stat status;

        if(argc == 1)
        {}
        else if(argc == 2)
        {//one file or dir
            snprintf(path, BUFF_SIZE, "%s/%s", cwd, argv[1]);
            stat(argv[1], &status);

            if(S_ISREG(status.st_mode))
            {
                printf("%s ", argv[1]);
            }
            else
            {
                list_dir(path, list_long, list_all, recursive);
            }
        }
        else
        {
            for(int i = optind; i <= argc - 1; i++)
            {
                snprintf(path, BUFF_SIZE, "%s/%s", cwd, argv[i]);
                test_file(path);

                stat(argv[i], &status);
                if(!is_dir(path))
                {
                    if(S_ISREG(status.st_mode))
                    {
                        printf("%s ", argv[i]);
                    }
                    printf("\n");
                } 
                else 
                {
                    if(!list_long && !list_all)
                    {
                        printf("%s:\n", argv[i]);
                    }
                    list_dir(path, list_long, list_all, recursive);
                }
            }
        }
    }

    if(recursive)
    {
        if(argc == 2)
        {
            printf(".:\n");
        }
        
        char cwd_buf[BUF_SIZE];
        strcpy(cwd_buf, cwd);
        struct stat status;

        char * ptr = strtok(cwd_buf, "/"); 
        while(ptr) 
        {
            dir_main = ptr; //dir_main contains current directory's name, substring after last '/' in cwd
            ptr = strtok(NULL, "/"); 
        }
        strcpy(dir_rec, dir_main);

        if(argc == 2)
        {// ./ls -R without additional args
            list_dir(cwd, list_long, list_all, recursive);
        }
        else
        {
            for(int i = optind; i <= argc - 1; i++)
            {
                snprintf(cwd_buf, BUFF_SIZE, "%s/%s", cwd, argv[i]);
                test_file(cwd_buf);

                stat(argv[i], &status);
                if(!is_dir(cwd_buf))
                {
                    if(S_ISREG(status.st_mode))
                    {
                        printf("%s ", argv[i]);
                    }
                    printf("\n");
                } 
                else 
                {
                    printf("%s:\n", argv[i]);
                    list_dir(cwd_buf, list_long, list_all, recursive);
                }

                if(i != argc - 1)
                {
                    printf("\n");
                }
            }
        }
        
    }
        
    exit(err_code);
    return 0;
    //NOT_YET_IMPLEMENTED("Listing files");
}
