#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    char c;
    int n;
    // a pair of pipes, one for each direction
    int parent_to_child[2];
    int child_to_parent[2];
    

    pipe(parent_to_child);
    pipe(child_to_parent);

    if (fork() == 0)    //child
    {
        close(parent_to_child[1]);
        close(child_to_parent[0]);
        
        n = read(parent_to_child[0], &c, 1);
        if (n != 1)
        {
            fprintf(2, "child read error\n");
            exit(1);
        }

        printf("%d: received ping\n", getpid());
        write(child_to_parent[1], &c, 1);
        
        close(parent_to_child[0]);
        close(child_to_parent[1]);
        exit(0);
    }
    else    //parent
    {
        close(parent_to_child[0]);
        close(child_to_parent[1]);
        
        write(parent_to_child[1], &c, 1);
        n = read(child_to_parent[0], &c, 1);
        if (n != 1)
        {
            fprintf(2, "parent read error\n");
            exit(1);
        }
        printf("%d: received pong\n", getpid());
        
        close(child_to_parent[0]);
        close(parent_to_child[1]);
        wait(0);
        exit(0);
    }
}