#include <fcntl.h>
#include <sys/types.h>
#include <iostream>
#include <fcntl.h> /* Obtain O_* constant definitions */
#include <unistd.h>
#include <vector>
#include "piped_async_command.hh"
#include "io_context.hh"
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>

PipedAsyncCommand::PipedAsyncCommand(std::string cmd, AsyncFileDescriptor *socket) : PipedAsyncCommand(cmd, socket->io_context_)
{
}

PipedAsyncCommand::PipedAsyncCommand(std::string cmd, IOContext &context)
{
    std::cout << "PipedAsyncCommand construction " << cmd << std::endl;
    //      parent        child
    //      fd1[1]        fd1[0]
    //        4 -- fd_w --> 3
    //      fd2[0]        fd2[1]
    //        5 <-- fd_r -- 6
    if (pipe(fd_w) == -1)
    {
        perror("Pipe Failed");
    }
    if (pipe(fd_r) == -1)
    {
        perror("Pipe Failed");
    }
    async_read_end_fd = std::make_shared<AsyncFileDescriptor>(fd_r[0], context);
    context.attachReadonly(async_read_end_fd.get());
    async_write_end_fd = std::make_shared<AsyncFileDescriptor>(fd_w[1], context);
    context.attachWriteOnly(async_write_end_fd.get());
    pid_t proces_id = fork();
    std::cout << "forked process ---- " << proces_id << "........................... " << std::endl;
    if (proces_id == -1)
    {
        perror("fork failed-------------");
        exit(EXIT_FAILURE);
    }
    if (proces_id == 0)
    { /* Child reads from pipe and writes back as soon as it finishes*/

        close(fd_w[1]);      // Close writing end of first pipe
        close(STDIN_FILENO); // closing stdin
        dup(fd_w[0]);        // replacing stdin with pipe read

        // Close both reading ends
        close(fd_w[0]);
        close(fd_r[0]);

        close(STDOUT_FILENO); // closing stdout
        dup(fd_r[1]);         // replacing stdout with pipe write
        close(fd_r[1]);

        // emulate an echo command by reading the contents of the pipe using cat
        std::vector<char *> argc;
        argc.emplace_back(const_cast<char *>(cmd.data()));
        // NULL terminate
        argc.push_back(nullptr);
        // The first argument to execvp should be the same as the
        // first element in argc
        execvp(argc.data()[0], argc.data());
        perror("execvp of \"cat\" failed");
        exit(1);
    }
    forked_proces_id = proces_id;
    close(fd_r[1]);
    close(fd_w[0]);
    std::cout << "PipedAsyncCommand creation finished " << std::endl;
}

PipedAsyncCommand::~PipedAsyncCommand()
{
    async_read_end_fd.reset();
    async_write_end_fd.reset();
    // wait(NULL); //this is important to prevent zombi process
    // wait will block the execution,

    // pid_t cpid = waitpid(forked_proces_id, NULL, WNOHANG);
    // wait pid returns -1 may be the process is not yet dead

    pid_t cpid = waitpid(forked_proces_id, NULL, WNOHANG);
    std::cout << "wait returned                              : " << cpid << " but waiting for " << forked_proces_id << std::endl;
    if (cpid == 0)
    {
        // some times with simultaneous clients the prcess does not die.
        // it is necesarry to kill it.
        int killret = kill(forked_proces_id, SIGKILL);
        cpid = waitpid(forked_proces_id, NULL, WNOHANG);
        std::cout << "kill returned " << killret << " wait returned                              : " << cpid << " but waiting for " << forked_proces_id << std::endl;
    }
    while (cpid == 0 || cpid == -1)
    {
        cpid = waitpid(forked_proces_id, NULL, WNOHANG);
        std::cout << "wait returned                              : " << cpid << " but waiting for " << forked_proces_id << std::endl;
    }

    // waitng for process of the group woks but waits for the previous process.
    // TODO need to implement a coro for this https://dxuuu.xyz/wait-pid.html
}

FileReadOperation PipedAsyncCommand::readpipe(void *buffer, std::size_t len)
{
    return FileReadOperation{async_read_end_fd, buffer, len};
}

FileWriteOperation PipedAsyncCommand::writepipe(void *buffer, std::size_t len)
{
    return FileWriteOperation{async_write_end_fd, buffer, len};
}
