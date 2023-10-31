/*
 * Shared State
 *
 * Copyright (C) 2023  Gioacchino Mazzurco <gio@eigenlab.org>
 * Copyright (c) 2023  Javier Jorge <jjorge@inti.gob.ar>
 * Copyright (c) 2023  Instituto Nacional de Tecnología Industrial
 * Copyright (C) 2023  Asociación Civil Altermundi <info@altermundi.net>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <map>
#include <memory>
#include <type_traits>
#include <fcntl.h>

#include "task.hh"
#include "async_file_desc.hh"
#include "close_operation.hh"

#include <util/rsdebug.h>
#include <util/stacktrace.h>
#include <util/rsdebuglevel2.h>


class Socket;
class PopenAsyncCommand;
class SocketAcceptOperation;
class SocketRecvOperation;
class SocketSendOperation;
class ReadOp;
class WriteOp;
class PopenFileReadOperation;
class PipedAsyncCommand;
class DyingProcessWaitOperation;
class ListeningSocket;
class ConnectOperation;
class ConnectingSocket;

/**
 * This class works as dispacher, notifying suspended blocksyscall when
 * ready to be activated. Blocksyscalls must suscribe to this class in order
 * to be notified.
 *
 * @brief This class is an Just an epoll wrapper 
 */
class IOContext
{
public:
	static std::unique_ptr<IOContext> setup(
	        std::error_condition* errc = nullptr );

	void run();

	template<class AFD_T, typename /* = AsyncFileDescriptor and derivatives */>
	std::shared_ptr<AFD_T> registerFD(
	        int fd, std::error_condition* errbub = nullptr );

	/**
	 * @tparam AFD_T AsyncFileDescriptor or derivatives
	 */
	template<class AFD_T>
	std::task<bool> closeAFD(
	        std::shared_ptr<AFD_T> aFD, std::error_condition* errbub = nullptr );

private:
	IOContext(int epollFD);

	static constexpr int DEFAULT_MAX_EVENTS = 20;

	const int mEpollFD;

    //TODO: verify if we still need processedSockets now that we have managed_fd 
    // Fill it by watchRead / watchWrite
	//std::set<AsyncFileDescriptor*> processedSockets;
    /**
     * @brief Register every managed file descriptor, controlled by attach and 
     * detach. 
     * 
     * Muchas veces sucede una race condition en la que un evento se 
     * dispara y luego se cierra el fd y se dessuscribe al fd de epool pero 
     * epool ya tiene registro del evento y nos intenta notificar. Esa 
     * notificación dispara una invocación de una a un resume (rec/send) y ya 
     * no existe corrutina que invocar. Eso se resolvia verificando la 
     * corrutina  if (!coroRecv_) o con el contadro de los asinc file descritpor if 
     * (number > 51 ) pero no es elegante y puede fallar. Este evento se podía
     * observar puntualmente cuando se espera a que un proceso muera. Se 
     * realiza la llamada a sistema, la misma no se suspende pues responde 
     * rápido, pero el fd esta suscripto a epoll, cuando se termina de procesar
     * la llamada el fd se elimina y se cierra. pero epoll nos envia una última
     * notificacón en el próximo run. Tambíen se podría realizar la suscripción
     *  solo despues de que la llamada se suspende pero eso podría dar lugar a 
     * otra race condition en la que la notificacion llegue al kernel antes de 
     * lograr la suscripción. mas info en illumos.org/man/7/epoll 
     * https://web.archive.org/web/20221231201027/https://illumos.org/man/7/epoll
     * Para resolver esto se implementó una lista de filedescriptors que nos 
     * interesa manejar con epoll y que cuando ya no nos intersa simplemente lo
     * quitamos de esa lista y con ello evitamos invocar un resume que ya no 
     * existe.
     */
	//std::set<AsyncFileDescriptor*> managed_fd;

    friend AsyncFileDescriptor;
	friend Socket;
    friend PopenAsyncCommand;
    friend SocketAcceptOperation;
    friend SocketRecvOperation;
    friend SocketSendOperation;
	friend ReadOp;
	friend WriteOp;
    friend PopenFileReadOperation;
    friend PipedAsyncCommand;
    friend DyingProcessWaitOperation;
	friend ListeningSocket;
	friend ConnectOperation;
	friend ConnectingSocket;

	// TODO: do we really need to make this API methods private?
	// TODO: Take a reference to an AsyncFileDescriptor instead of a pointer
	// TODO: This methods can fail add error bubbling paramether
    void attach(AsyncFileDescriptor* socket);
    void attachReadonly(AsyncFileDescriptor* socket);
    void attachWriteOnly(AsyncFileDescriptor* socket);
    void watchRead(AsyncFileDescriptor* socket);
    void unwatchRead(AsyncFileDescriptor* socket);
    void watchWrite(AsyncFileDescriptor* socket);
    void unwatchWrite(AsyncFileDescriptor* socket);
#if 0
	void detach(
	        AsyncFileDescriptor* aFD,
	        std::error_condition* errbub = nullptr );

	/** Like detach, but for externally closed FD skip asking epoll processing
	 * which is done automatically and would fail on already closed FD */
	void discard(AsyncFileDescriptor& aFD);
#endif

	/** Map OS file descriptor to managed AsyncFileDescriptor */
	std::map<int, std::shared_ptr<AsyncFileDescriptor>> mManagedFD;
};


template<class AFD_T = AsyncFileDescriptor,
         typename = std::enable_if_t<std::is_base_of_v<AsyncFileDescriptor, AFD_T>>>
std::shared_ptr<AFD_T> IOContext::registerFD(
        int fd, std::error_condition* errbub )
{
	auto flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	if(flags == -1)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            " failure getting FD: ", fd, " flags" );
		return nullptr;
	}

	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            " failure setting FD: ", fd, " non-blocking" );
		return nullptr;
	}

	auto aFD = std::shared_ptr<AFD_T>(
	            new AFD_T(fd, *this) );
	mManagedFD[fd] = aFD;

	return aFD;
}

template<class AFD_T>
std::task<bool> IOContext::closeAFD(
        std::shared_ptr<AFD_T> aFD,
        std::error_condition* errbub )
{
	static_assert( std::is_base_of<AsyncFileDescriptor, AFD_T>::value,
	        "AsyncFileDescriptor or derivative required");
	static_assert(!std:: is_same<PipedAsyncCommand, AFD_T>::value,
	        "PipedAsyncCommand have its own specialization");

	auto sysCloseErr = co_await CloseOperation(*aFD.get(), errbub);

	if(sysCloseErr)
	{
		rs_error_bubble_or_exit(
		            rs_errno_to_condition(errno), errbub,
		            "failure closing ", *aFD.get() );
		co_return false;
	}

	auto fdBack = aFD->mFD;
	aFD->mFD = -1;
	co_return mManagedFD.erase(fdBack);
}
