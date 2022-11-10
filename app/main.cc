#include "io_context.hh"
#include "socket.hh"
#include "task.hh"
#include "sharedstate.hh"
#include <iostream>
#include <array>
#include <unistd.h>

// Executables must have the following defined if the library contains
// doctest definitions. For builds with this disabled, e.g. code shipped to
// users, this can be left out.
#ifdef ENABLE_DOCTEST_IN_LIBRARY
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#endif

std::task<bool> inside_loop(Socket &socket)
// tomamos esto, hacemos el merge y lo devolvemos
{
    char buffer[42] = {0};
    ssize_t nbRecv = co_await socket.recv(buffer, (sizeof buffer)-1);
    ssize_t nbSend = 0;
    // TODO: recibir todo enviarlo a algo que llamamos  al stdout
    // TODO: crear una task que invoque al shstate empezar por invocar echo.

    std::cout << "RECIVING (" << buffer << "):" << '\n';
    std::string merged = SharedState::mergestate(buffer);
    //std::string merged;
    //mergestate(buffer, merged);
    // esto no parece necesario
    // while (nbSend < nbRecv)
    //{
    std::cout << "SENDINGS (" << merged << "):" << '\n';

    std::cout << "SENDING Ss (" << merged.size() << "):" << '\n';

    ssize_t res = co_await socket.send(merged.data(), merged.size());
    if (res <= 0)
        co_return false;
    nbSend += res;
    //}
    // esto va al std error
    std::cout << "DONE (" << nbRecv << "):" << '\n';
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
