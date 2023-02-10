#include "io_context.hh"
#include "socket.hh"
#include "task.hh"
#include "sharedstate.hh"
#include <iostream>
#include <array>
#include <unistd.h>
#include "async_command.hh"
#include "piped_async_command.hh"

#define BUFFSIZE 256

/// @brief coro in charge of information handling. It takes the received states, merges it and return the updated status using the socket.
/// @param socket
/// @return true if everything goes fine
std::task<bool> inside_loop(Socket &socket)
{
    char socbuffer[BUFFSIZE] = {0};
    // TODO: lo que no entra en el buffer se procesa como otro mensaje...
    ssize_t nbRecv = co_await socket.recv(socbuffer, (sizeof socbuffer) - 1);
    if (nbRecv <= 0)
    {
        co_return false;
    }
    std::cout << "RECIVING (" << socbuffer << "):" << '\n';
    std::array<char, BUFFSIZE> buffer;
    std::string merged;
    std::string cmd = "sleep 1 && echo '" + std::string(socbuffer) + "'";
    std::unique_ptr<PipedAsyncCommand> asyncecho = std::make_unique<PipedAsyncCommand>("cat", &socket);
    co_await asyncecho->writepipe(socbuffer, nbRecv);
    std::cout << "writepipe (" << socbuffer << "):" << '\n';
    co_await asyncecho->readpipe(buffer.data(), BUFFSIZE);
    merged = buffer.data();
    std::cout << "readpipe (" << merged << "):" << '\n';
    asyncecho.reset(nullptr);
    // problemade manejo de errores... que pasa cuando se cuelgan los endpoints y ya no reciben.
    // sin esta linea se genera un enter que no se recibe y el programa explota
    merged.erase(std::remove(merged.begin(), merged.end(), '\n'), merged.cend());
    size_t nbSend = 0;
    while (nbSend < merged.size()) // probar y hacer un pull request al creador
    {
        std::cout << "SENDING (" << merged << "):" << '\n';
        ssize_t res = co_await socket.send(&(merged.data()[nbSend]), merged.size() - nbSend);
        if (res <= 0)
        {
            std::cout << "DONE (" << nbRecv << "):" << '\n';
            co_return false;
        }
        nbSend += res;
    }
    // TODO: esto va al std error ?? SERA QUE PODEMOS USAR UNA LIBRERIA DE LOGGFILE
    std::cout << "DONE (" << nbRecv << "):" << '\n';
    co_return false;
}

std::task<bool> echo_socket(std::unique_ptr<Socket> socket)
{
    bool run = true;
    while (run)
    {
        std::cout << "BEGIN\n";
        run = co_await inside_loop(*socket);
        std::cout << "END\n";
    }
    socket.reset(nullptr);
    co_return true;
}

std::task<> accept(Socket &listen)
{
    while (true)
    {
        std::cout << "begin accept\n";
        auto socket = co_await listen.accept();
        auto t = echo_socket(std::move(socket));
        t.make_disposable();
        t.resume();
        std::cout << "end accept\n";
        // when the loop ends the task t is destroyed, if the task is not
        // disposable te promise type and al the resources are freed
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
