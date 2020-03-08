// TcpConnection.cc
// Created by Lixin on 2020.02.23

#include "main/net/TcpConnection.h"

#include "main/base/Logging.h"
#include "main/net/Channel.h"
#include "main/net/EventLoop.h"
#include "main/net/Socket.h"
#include "mian/net/SocketsOps.h"

#include <errno.h>

using namespace main;
using namespace main::net;

void muduo::net::defaultConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
            << conn->peerAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
  // do not call conn->forceClose(), because some users want to register message callback only.
}

void muduo::net::defaultMessageCallback(const TcpConnectionPtr&,
                                        Buffer* buf,
                                        Timestamp)
{
  buf->retrieveAll();
}

TcpConnection::TcpConnection(EventLoop* loop,
							const string& nameArg,
							int sockfd,
							const InetAddress& localAddr,
							const InetAddress& peerAddr)
	: loop_(CHECK_NOTNULL(loop)),
	  name_(nameArg),
	  state_(kConnecting),
	  reading_(true),
	  socket(new Socket(sockfd)),
	  channel_(new Channel(loop,sockfd)),
	  localAddr_(localAddr),
	  peerAddr_(peerAddr),
	  highWaterMark_(64*1024*1024)
{
	channel_->setReadCallback(
		std::bind(&TcpConnection::handleRead,this,_1));
	channel_->setWriteCallback(
      std::bind(&TcpConnection::handleWrite, this));
  	channel_->setCloseCallback(
      std::bind(&TcpConnection::handleClose, this));
  	channel_->setErrorCallback(
      std::bind(&TcpConnection::handleError, this));
	LOG_DEBUG << "TcpConnection::ctor[" << name_ << "] at" << this
				<< " fd=" << sockfd;
	socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
	LOG_DEBUG << "TcpConnection::dtor[" <<  name_ << "] at " << this
            << " fd=" << channel_->fd()
            << " state=" << stateToString();
	assert(state_ == kDisconnected);
}

bool TcpConnection::getTcpInfo(struct tcp_info* tcpi) const
{
	return socket_->getTcpInfo(tcpi);
}

string TcpConnection::getTcpInfoString() const
{
	char buf[1024];
	buf[0] = '\0';
	socket_->getTcpInfoString(buf,sizeof(buf));
	return buf;
}

void TcpConnection::send(const void* data,int len)
{
	send(string(static_cast<const char*><data>,len));
}
void TcpConnection::send(const string& message)
{
  if (state_ == kConnected)
  {
    if (loop_->isInLoopThread())
    {
      sendInLoop(message);
    }
    else
    {
      void (TcpConnection::*fp)(const string& message) = &TcpConnection::sendInLoop;
      loop_->runInLoop(
          std::bind(fp,
                    this,     // FIXME
                    message.as_string()));
                    //std::forward<string>(message)));
    }
  }
}

void TcpConnection::send(Buffer* buf)
{
  if (state_ == kConnected)
  {
    if (loop_->isInLoopThread())
    {
      sendInLoop(buf->peek(), buf->readableBytes());
      buf->retrieveAll();
    }
    else
    {
      void (TcpConnection::*fp)(const string& message) = &TcpConnection::sendInLoop;
      loop_->runInLoop(
          std::bind(fp,
                    this,     // FIXME
                    buf->retrieveAllAsString()));
                    //std::forward<string>(message)));
    }
  }
}

void TcpConnection::sendInLoop(const string& message)
{
  sendInLoop(message.data(), message.size());
}

void TcpConnection::sendInLoop(const void* data,size_t len)
{
	loop_->assertInLoop();
	ssize_t nwrote = 0;
	size_t remaining = len;
	bool faultError = false;
	if(state_ == kDisconnected)
	{
		LOG_WARM << "disconnected,give up writing";
		return ;
	}
	// if no thing in output queue, try writing directly
	if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
	{
		nwrote = sockets::write(channel_->fd(),data,len);
		if(nwrote >= 0)
		{
			remaining = len - nwrote;
			if( remaining == 0 && writeCompleteCallback_)
			{
				loop_->queueInLoop(std::bind(writeCompleteCallback_,
									shared_from_this()));
			}
		}
		else // nwrote < 0
		{
			nwrote = 0;
			if(errno != EWOULDBLOCK)
			{
				LOG_SYSERR << "TcpConnection::sendInLoop";
				if(errno == EPIPE || errno == ECONNRESET) 
				{
					faultError = true;
				}
			}
		}
	}

	assert(remaining <= len);
	if(!faultError && remaining > 0)
	{
		size_t oldLen = outputBuffer_.readableBytes();
		if(oldLen + remaining >= highWaterMark_
			&& oldLen < highWaterMark_
			&& highWaterMarkCallback_)
		{
			loop_->queueInLoop(std::bind(highWaterMarkCallback_,
											shared_from_this(),
											oldLen + remaining ));
		}
		outoutBuffer_.append(static_cast<const char*>(data)+nwrote,remaining);
		if(!channel_->isWriting())
		{
			channel_->enablWriting();
		}
	}
}

void TcpConnection::shutdown()
{
	// TODO : use compare and swap
	if(state_ = kConnected)
	{
	 setState(kDisconnecting);
	 // FIXME: shared_from_this()?
	 loop_->runInloop(std::bind(&shutdownInLoop,this));
	}
}

void TcpConnection::shutdownInLoop()
{
	loop_->assertInLoopThread();
	if(!channel_->isWriting())
	{
		socket_->shutdownWrite();
	}
}

void TcpConnection::forceClose()
{
	// TODO: use compare and swap
	if(state_ == kConnected || state_ == kDisconnecting)
	{
		setState(kDisconnecting);
		loop_->queueInLoop(std::bind(&TcpConnection::forceInLoop,shared_from_this()));
	}
}

void TcpConnection::forceCloseWithDelay(double second)
{
	if(state_ == kConnected || state_ == kDisconnecting)
	{
		setState(kDisconnection);
		loop_->runAfter(second,
						makeWeakCallback(shared_from_this(),
											&TcpConnection::forceClose));
									// not forceCloseInLoop,to avoid race condition
	}
}

void TcpConnection::forceCloseInLoop()
{
	loop_->assertInLoopThread();
	if(state_ == kConnected || state_ == kDisconnecting)
	{
		// as if we received 0 byte in handleRead();
		handleClose();
	}
}

const char* TcpConnection::stateToString() const
{
  switch (state_)
  {
    case kDisconnected:
      return "kDisconnected";
    case kConnecting:
      return "kConnecting";
    case kConnected:
      return "kConnected";
    case kDisconnecting:
      return "kDisconnecting";
    default:
      return "unknown state";
  }
}

void TcpConnection::setTcpNoDelay(bool on)
{
	socket_->setTcpNoDelay(on);
}

void TcpConnection::startRead()
{
	loop_->runInLoop(std::bind(&TcpConnection::startReadInLoop,this));
}

void TcpConnection::startReadInLoop()
{
	loop_->assertInLoopThread();
	if(!reading_ || !channel_->isReading())
	{
		channel_->enableReading();
		reading_ = true;
	}
}

void TcpConnection::stopRead()
{
	loop_->runInLoop(std::bind(&TcpConnection::stopReadInLoop, this));
}

void TcpConnection::stopReadInLoop()
{
	loop_->assertInLoopThread();
  	if (reading_ || channel_->isReading())
  	{
    	channel_->disableReading();
    	reading_ = false;
  	}
}

void TcpConnection::connectEstablished()
{
	loop_->assertInLoopThread();
	assert(state_ == kConnecting);
	setState(kConnected);
	channel_->tie(shared_from_this());
	channel_->enableReading();

	connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed()
{
	loop_->assertInLoopThread();
	if(state_ == kConnected)
	{
		setState(kDisconnected);
		channel_->disableAll();

		connectionCallback_(shared_from_this());
	};
	channel_->remove();
}

void TcpConnection::handleWrite()
{
	loop_->assertInLoopThread();
	if(channel_->isWriting())
	{
		ssize_t n = ::write(channel_->fd(),
							outputBuffer_.peek(),
							outputBuffer_.readableBytes());
		if(n > 0)
		{
			outputBuffer_.retrieve(n);
			if(outputBuffer_.readableBytes() == 0)
			{
				channel_->disableWriting();
				if(state_ == writeCompleteCallback_)
				{
					loop_->queueInLoop(std::bind(writeCompleteCallback_,
										shared_from_this()));
				}
				if(state_ == kDiconnecting)
				{
					shutdownInLoop();
				}
			} 
		}
		else
		{
			LOG_SYSERR << "TcpConnection::handleWrite";
		}
	}
	else
	{
		LOG_TRACE << "Connection fd = " << channel_->fd()
					<< " is down, no more writing";
	}
	
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
	loop_->assertInLoopThread();
	int savedErrno = 0;
	ssize_t n = inputBuffer_.readFd(channel_->fd(),&saveErrno);
	if(n > 0)
	{
		messageCallback_(shared_from_this(),&inputBuffer_,receriveTime);
	}
	else if (n == 0)
	{
		handleClose();
	}
	else
	{
		errno = savedErrno;
		LOG_STSERR << "TcpConnection::handleRead";
		handleError();
	}
}

void TcpConnection::handleClose()
{
	loop_->assertInLoopThread();
	LOG_TRACE << "fd = " << channel_->fd() << " state = " << stateToString();
	assert(state_ == kConnected || state_ == kConnecting);
	// don't need to close fd, leave it to dtor
	setState(kDisconnected);
	channel_->disableAll();

	TcpConnectionPtr guardThis(shared_from_this());
	connectionCallback_(guardThis);
	closeCallback_(guardThis);
}

void TcpConnection::handleError()
{
	int err = sockets::getSocketError(channel->fd());
	LOG_ERROR << "TcpConnection::handleError [" << name_
				<< "] - SO_ERROR = " << err << " " << strerror_tl(err);
}


