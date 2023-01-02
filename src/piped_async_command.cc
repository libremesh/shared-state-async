#include <fcntl.h>
#include <sys/types.h>
#include <iostream>
#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>
#include <vector>
#include "piped_async_command.hh"


PipedAsyncCommand::PipedAsyncCommand(std::string cmd, AsyncFileDescriptor *socket)
{
    //      parent        child
    //      fd1[1]        fd1[0]
    //        4 -- fd_w --> 3 
    //      fd2[0]        fd2[1]
    //        5 <-- fd_r -- 6 
    std::cout<< cmd;
    if (pipe(fd_w) == -1)
    {
        perror("Pipe Failed");
    }
    if (pipe(fd_r) == -1)
    {
        perror("Pipe Failed");
    }
    async_read_end_fd = new AsyncFileDescriptor(fd_r[0],socket->io_context_);
    async_write_end_fd = new AsyncFileDescriptor(fd_w[1],socket->io_context_);
    pid_t cpid = fork();
    if (cpid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (cpid == 0) {    /* Child reads from pipe and writes back as soon as it finishes*/
        close(fd_w[1]); // Close writing end of first pipe
        close(STDIN_FILENO);   //closing stdin
        dup(fd_w[0]);           //replacing stdin with pipe read 

        // Close both reading ends
        close(fd_w[0]);
        close(fd_r[0]);

        close(STDOUT_FILENO);  //closing stdout
        dup(fd_r[1]);           //replacing stdout with pipe write 
        close(fd_r[1]);
        
        //emulate an echo command by reading the contents of the pipe using cat
        std::vector<char*> argc;
        // const_cast is needed because execvp prototype wants an
        // array of char*, not const char*.
        argc.emplace_back(const_cast<char*>("cat"));
        // NULL terminate
        argc.push_back(nullptr);
        // The first argument to execvp should be the same as the
        // first element in argc
        execvp(argc.data()[0],argc.data());
        perror("execvp of cat failed");
        exit(1);
    }
}

PipedAsyncCommand::~PipedAsyncCommand()
{

}

FileReadOperation PipedAsyncCommand::readpipe(void *buffer, std::size_t len)
{
    return FileReadOperation{async_read_end_fd, buffer, len};
}

FileWriteOperation PipedAsyncCommand::writepipe(void* buffer, std::size_t len)
{
    return FileWriteOperation{async_write_end_fd, buffer, len};
}
