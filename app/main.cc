#include "io_context.hh"
#include "socket.hh"
#include "task.hh"
#include "sharedstate.hh"
#include <iostream>
#include <array>
#include <unistd.h>

/// @brief coro in charge of information handling. It takes the received states, merges it and return the updated status using the socket.
/// @param socket 
/// @return true if everything goes fine
std::task<bool> inside_loop(Socket &socket)
{
    char buffer[42] = {0};
    ssize_t nbRecv = co_await socket.recv(buffer, (sizeof buffer)-1);
    //ssize_t nbSend = 0;
    // TODO: crear una task que invoque al shstate empezar por invocar echo.????
    std::cout << "RECIVING (" << buffer << "):" << '\n';
    std::string merged = SharedState::mergestate(buffer);

    // esto no parece necesario, podria quedarse aqui para siempre  ? 
    // while (nbSend < nbRecv)
    //{
    std::cout << "SENDING (" << merged << "):" << '\n';
    ssize_t res = co_await socket.send(merged.data(), merged.size());
    if (res <= 0)
        co_return false;
    //nbSend += res;
    //}
    //TODO: esto va al std error ??
    std::cerr << "DONE (" << nbRecv << "):" << '\n';
    if (nbRecv <= 0)
        co_return false;
    printf("%s\n", buffer);
    co_return true;
}

std::task<> echo_socket(std::shared_ptr<Socket> socket)
{
    bool run = true;
    while (run)
    {
        std::cout << "BEGIN\n";
        run = co_await inside_loop(*socket);
        std::cout << "END\n";
    }
}

std::task<> accept(Socket &listen)
{
    while (true)
    {
        auto socket = co_await listen.accept();
        auto t = echo_socket(socket);
        t.resume();
    }
}

int main()
{
    IOContext io_context{};
    Socket listen{"3490", io_context};
    auto t = accept(listen);
    t.resume();

    io_context.run();
}