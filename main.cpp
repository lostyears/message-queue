#include <cstdlib>
#include <iostream>
#include <queue>
#include <string>
#include <mutex>
#include <boost/format.hpp>   
#include <boost/tokenizer.hpp> 
#include <boost/algorithm/string.hpp> 
#include <boost/bind.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
const std::string c_command_push = "PUSH";
const std::string c_command_pop = "POP";
const std::string c_delim = "\r\n";

class msg_queue
{

public:
	static msg_queue& get_instance()
	{
		static msg_queue _obj;
		return _obj;
	}

	void push(const std::string& msg)
	{
		que_.push(msg);
	}

	bool pop(std::string& msg)
	{
		if (!que_.empty())
		{
			msg = que_.front();
			que_.pop();
			return true;
		}

		return false;
	}
private:
	msg_queue(){}

protected:
	std::queue<std::string> que_;
};

class session
{
public:
	session(boost::asio::io_service& io_service)
		: socket_(io_service)
		, response_("You are connected." + c_delim)
	{
	}

	tcp::socket& socket()
	{
		return socket_;
	}

	void start()
	{
		boost::asio::async_write(socket_,
			boost::asio::buffer(response_),
			boost::bind(&session::handle_write, this,
			boost::asio::placeholders::error));
	}

private:
	void handle_read(const boost::system::error_code& error,
		size_t bytes_transferred)
	{
		if (!error)
		{
			boost::asio::streambuf::const_buffers_type bufs = sbuf_.data();
			std::string line(
				boost::asio::buffers_begin(bufs),
				boost::asio::buffers_end(bufs));
			sbuf_.consume(bytes_transferred);

			std::vector<std::string> segs;
			boost::split(segs, line, boost::is_any_of(" "));
			boost::to_upper(segs[0]);
			boost::trim_right_if(segs[0], boost::is_any_of(c_delim));
			if (segs[0] == c_command_pop)
			{
				if (!msg_queue::get_instance().pop(response_))
				{
					response_ = "The message queue is empty now.";
				}

				response_ += c_delim;
				boost::asio::async_write(socket_,
					boost::asio::buffer(response_),
					boost::bind(&session::handle_write, this,
					boost::asio::placeholders::error));

				return;
			}

			if (segs[0] == c_command_push)
			{
				line = line.substr(c_command_push.length(), std::string::npos);
				boost::trim_left_if(line, boost::is_any_of(" "));
				boost::trim_right_if(line, boost::is_any_of(c_delim));
				msg_queue::get_instance().push(line);
			}

			boost::asio::async_read_until(socket_,
				sbuf_,
				c_delim,
				boost::bind(&session::handle_read,
				this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
		}
		else
		{
			delete this;
		}
	}

	void handle_write(const boost::system::error_code& error)
	{
		if (!error)
		{
			boost::asio::async_read_until(socket_,
				sbuf_,
				c_delim,
				boost::bind(&session::handle_read,
				this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
		}
		else
		{
			delete this;
		}
	}

	tcp::socket socket_;
	boost::asio::streambuf sbuf_;
	std::string response_;
};

class server
{
public:
	server(boost::asio::io_service& io_service, short port)
		: io_service_(io_service),
		acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
	{
		start_accept();
	}

private:
	void start_accept()
	{
		session* new_session = new session(io_service_);
		acceptor_.async_accept(new_session->socket(),
			boost::bind(&server::handle_accept, this, new_session,
			boost::asio::placeholders::error));
	}

	void handle_accept(session* new_session,
		const boost::system::error_code& error)
	{
		if (!error)
		{
			new_session->start();
		}
		else
		{
			delete new_session;
		}

		start_accept();
	}

	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
	try
	{
		if (argc != 2)
		{
			std::cerr << "Usage: msg_queue <port>\n";
			return 1;
		}

		boost::asio::io_service io_service;

		server s(io_service, std::stoi(argv[1]));

		io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}




