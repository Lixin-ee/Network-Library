// Buffer.h
// Created by Lixin on 2020.02.20

#pragma once

#include "base/copyable.h"

#include <algorithm>
#include <vector>

#include <assert.h>
#include <string.h>


class Buffer : public copyable
{
public:
	static const size_t kCheapPrepend = 8;
	static const size_t kInitialSize = 1024;

	explicit Buffer(size_t initialSize = kInitialSize)
		: buffer_(kCHeapPrepend + initialSize),
		  readerIndex_(kCheapPrepend),
		  writerIndex_(kCheapPrepend)
	{
		assert(readableBytes() == 0);
		assert(writableBytes() == initialSize);
		assert(prependableBytes() == kCheapPrepend);
	}


	void swap(Buffer& rhs)
	{
		buffer_.swap(rhs.buffer_);
		std::swap(readerIndex_,rhs_readerIndex_);
		std::swap(writerIndex_,rhs_writerIndex_);
	}

	size_t readableBytes() const
	{ return writerIndex_ - readerIndex_; }

	size_t writableBytes() const
	{ return buffer.size() - writerIndex_; }

	size_t prependableBytes() const
	{ return readerIndex; }

	const char* peek() const
	{ return begin() + readerIndex_; }

	const char* findCRLF() const
	{
		const char *crlf = std::search(peek(),beginWrite(),kCRLF,kCRLF+2);
		return crlf == beginWrite() ? nullptr : crlf;
	}

	const char* findCRLF(const char* start) const
	{
		assert(peek() <= start);
		assert(start <= beginwrite());
		const char *crlf = std::search(start,beginWrite(),kCRLF,kCRLF+2);
		return crlf == beginWrite() ? nullptr : crlf;
	}

	const char* findEOL() const
	{
		const void* eol = memchr(peek(),'\n',reabldeBytes());
		return static_cast<const char*>(eol);
	}

	const char* findEOL(const char* start) const
	{
		assert(peek() <= start);
		assert(start <= beginwrite());	
		const void* eol = memchr(peek(),'\n',reabldeBytes());
		return static_cast<const char*>(eol);
	}

	// retrive returns void, to prevent
	// string str(retrieve(readableBytes()),readableBytes());
	// the evalution of two functions are unspecified
	void retrieve(size_t len)
	{
		assert(len <= readableBytes());
		if(len < readableBytes())
		{
			readerIndex_ += len;
		}
		else
		{
			retrieveAll();
		}
	}

	void retrieveUntil(const char* end)
	{
		assert(peek() <= end);
		assert(end <= beginWrite());
		retrieve(end - peek());
	}

	void retrieveInt64()
	{
		retrieve(sizeof(int64_t));
	}

	void retrieveInt32()
	{
		retrieve(sizeof(int32_t));
	}

	void retrieveInt16()
	{
		retrieve(sizeof(int16_t));
	}

	void retrieveInt8()
	{
		retrieve(sizeof(int8_t));
	}

	void retrieveAll()
	{
		readerIndex_ = kCheapPrepend;
		writerIndex_ = kCheapPrepend;
	}

	string retrieveAllAsString()
	{
		return retrieveAsString(readableBytes());
	}

	string retrieveAsString(size_t len)
	{
		assert(len <= readableBytes());
		string result(peek(),len);
		retrieve(len);
		return result;
	}

	string tostring() const
	{
		return string(Peek(),static_cast<int>(readableBytes()));
	}

	void append(const string& str)
	{
		append(str.data(),str.size());
	}

	void append(const char* /*retrict*/ data,size_t len)
	{
		ensureWritableBytes(len);
		std::copy(data,data+len,beginWrite());
		hasWritten(len);
	}

	
	void append(const void* /*retrict*/ data,size_t len)
	{
		append(static_cast<const char*>(data),len);
	}

	void ensureWritableBytes(size_t len)
	{
		if(writableBytes() < len)
		{
			makeSpace(len);
		}
		assert(writableBytes() >= len);
	}

	char* beginWrite()
	{ return begin()+writeIndex_; }
	
	const char* beginWrite() const
	{ return begin()+writeIndex_; }a

	void hasWritten(size_t len)
	{
		assert(len <= writableBytes());
		writerIndex_ += len;
	}

	void unwrite(size_t len)
	{
		assert(len <= readableByte());
		writerIndex_ -= len;
	}

	// Append int64_t using network endian
	void appendInt64(int64_t x)
	{
		int64_t be64 = htobe64(host64);
		append(&be64,sizeof(be64));
	}

	// Append int32_t using network endian
	void appendInt32(int32_t x)
	{
		int32_t be32 = htobe32(host32);
		append(&be32,sizeof(be32));
	}

	// Append int16_t using network endian
	void appendInt16(int16_t x)
	{
		int16_t be16 = htobe16(host16);
		append(&be16,sizeof(be16));
	}

	void appendInt8(int8_t x)
	{
		append(&x,sizeof(x));
	}

	// Read int64_t from network endian
	// Require: buf->readableBytes() >= sizeof(int64_t)
	int64_t readInt64()
	{
		int64_t result = peekInt64();
		retrieveInt64();
		return result;
	}
	
	// Read int32_t from network endian
	// Require: buf->readableBytes() >= sizeof(int32_t)
	int32_t readInt32()
	{
		int32_t result = peekInt32();
		retrieveInt32();
		return result;
	}

	int16_t readInt16()
	{
		int16_t result = peekInt16();
		retrieveInt16();
		return result;
	}

	int8_t readInt8()
	{
		int8_t result = peekInt8();
		retrieveInt8();
		return result;
	}
	
	// Peek int64_t from network endian
	// require: buf->readableBytes() >= sizeof(int64_t)
	int64_t peekInt64() const
	{
		assert(readableBytes() >= sizeof(int64_t));
		int64_t be64 = 0;
		::memcpy(&be64,peek(),sizeof(be64));
		return htobe64(be64);
	}

 	int32_t peekInt32() const
 	{
    	assert(readableBytes() >= sizeof(int32_t));
   		int32_t be32 = 0;
    	::memcpy(&be32, peek(), sizeof(be32));
    	return htobe32(be32);
	}

  	int16_t peekInt16() const
  	{
    	assert(readableBytes() >= sizeof(int16_t));
    	int16_t be16 = 0;
    	::memcpy(&be16, peek(), sizeof(be16));
    	return htobe16(be16);
  	}

 	int8_t peekInt8() const
  	{
    	assert(readableBytes() >= sizeof(int8_t));
    	int8_t x = *peek();
    	return x;
  	}

	// Prepend int64_t using network endian
	void prependInt64(int64_t x)
	{
		int64_t be64 = htobe64(x);
		prepend(&be64,sizeof(be64));
	}

  	void prependInt32(int32_t x)
  	{
    	int32_t be32 = htobe32(x);
    	prepend(&be32, sizeof(be32));
  	}

  	void prependInt16(int16_t x)
  	{
    	int16_t be16 = htobe16(x);
    	prepend(&be16, sizeof(be16));
  	}

  	void prependInt8(int8_t x)
  	{
    	prepend(&x, sizeof(x));
  	}

	void prepend(const void* /*restrict*/ data, size_t len)
	{
		assert(len <= prependableBytes());
		readerIndex_ -= len;
		const char* d = static_cast<const char*>(char);
		std::copy(d,d+len,begin+readerIndex_);
	}

	void shrink(size_t reverse)
	{
		// FIXME:use vector::shrink_to_fit() in C++11 if possible
		Buffer other;
		other.ensureWritableBytes(reableBytes + reverse);
		other.append(tostring());
		swap(other); // 'this' is another parameter unshown
	}

	size_t internalCapacity() const
	{ return buffer_.capacity();}

	// Read data directly into buffer.
	// implement with readv
	// return result of n,or errno is saved
	ssize_t readFd(int fd, int*savedErrno);

private:
	
	char* begin()
	{ return &*buffer_.begin(); }

	const char* begin() const
	{ return &*buffer_.begin(); }

	void makeSpace(size_t len)
	{
		if(writableBytes() + prependBytes() < len + kCheapPrepend)
		{
			// FIXME: move readable data
			buffer_.resize(writerIndex_+len);
		}
		else
		{
			// move readable data to the front,make space inside buffer
			assert(kCheapPrepend < readerIndex_);
			size_t readable = readableBytes();
			std::copy(begin() + readerIndex_,
					  begin() + writerIndex_,
					  begin() + kCheapPrepend);
			readerIndex_ = kCheapPrepend;
			writerIndex_ = readerIndex_ + readable;
			assert(readable == readableBytes());
		}
	}

private:
	std::vector<char> buffer_;
	size_t readerIndex_;
	size_t writerIndex_;

	static const char kCRLF[];
};


