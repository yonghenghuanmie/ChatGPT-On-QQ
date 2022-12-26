#include <cstdlib>
#include <deque>
#include <format>
#include <string>
#include <iostream>
#include <functional>
#include <string_view>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include "ProducerConsumerQueue.h"

using namespace nlohmann::literals;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

#undef SendMessage
#include "cqhttp.h"

//------------------------------------------------------------------------------

// Report a failure
void fail(beast::error_code ec, char const* what)
{
	std::cerr << what << ": " << ec.message() << "\n";
}

bool Filter::FilterMessage(beast::flat_buffer& buffer)
{
	try
	{
		json result = json::parse(std::string_view((const char*)buffer.data().data(), buffer.data().size()));
		if (result.contains("meta_event_type") && filter_set.contains(result["meta_event_type"]))
		{
			buffer.clear();
			return true;
		}
	}
	catch (std::exception e)
	{
		fail(boost::system::errc::make_error_code(boost::system::errc::invalid_argument), e.what());
	}
	return false;
}

template<typename TransferProtocol>
void MessageDispatcher::SendMessage(
	websocket::stream<TransferProtocol>& ws,
	net::yield_context yield
)
{
	beast::error_code ec;

	auto optional = out_queue.TryPop();
	if (optional)
	{
		json data = {
			{"action", "send_msg"},
			{"params", {
				{"group_id", optional->group_id},
				{"message", AtMessage(optional->user_id,/*std::move(optional->message)*/"Developing...")}
			}},
			{"echo", "not used"}
		};

		ws.async_write(net::buffer(data.dump()), yield[ec]);
		if (ec)
			fail(ec, "write");
	}
}

/*
	{"anonymous":null,"font":0,"group_id":1095366898,
	"message":"[CQ:at,qq=3484853390] 2","message_id":891987084,"message_seq":52660,"message_type":"group","post_type":"message","raw_message":"[CQ:at,qq=3484853390] 2","self_id":3484853390,
	"sender":{"age":0,"area":"","card":"","level":"","nickname":"......","role":"admin","sex":"unknown","title":"","user_id":1534971783},
	"sub_type":"normal","time":1672059301,"user_id":1534971783}
*/
void MessageDispatcher::ProcessMessage(beast::flat_buffer& buffer)
{
	try
	{
		json result = json::parse(std::string_view((const char*)buffer.data().data(), buffer.data().size()));
		if (result.contains("message_type") && result["message_type"] == "group")
			ParseMessage(result);
	}
	catch (std::exception e)
	{
		fail(boost::system::errc::make_error_code(boost::system::errc::invalid_argument), e.what());
	}
}

// Sends a WebSocket message and prints the response
void do_session(
	std::string host,
	std::string const& port,
	std::string const& text,
	net::io_context& ioc,
	net::yield_context yield)
{
	beast::error_code ec;

	// These objects perform our I/O
	tcp::resolver resolver(ioc);
	websocket::stream<beast::tcp_stream> ws(ioc);

	// Look up the domain name
	auto const results = resolver.async_resolve(host, port, yield[ec]);
	if (ec)
		return fail(ec, "resolve");

	// Set a timeout on the operation
	beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(30));

	// Make the connection on the IP address we get from a lookup
	auto ep = beast::get_lowest_layer(ws).async_connect(results, yield[ec]);
	if (ec)
		return fail(ec, "connect");

	// Update the host_ string. This will provide the value of the
	// Host HTTP header during the WebSocket handshake.
	// See https://tools.ietf.org/html/rfc7230#section-5.4
	host += ':' + std::to_string(ep.port());

	// Turn off the timeout on the tcp_stream, because
	// the websocket stream has its own timeout system.
	beast::get_lowest_layer(ws).expires_never();

	// Set suggested timeout settings for the websocket
	ws.set_option(
		websocket::stream_base::timeout::suggested(
			beast::role_type::client));

	// Set a decorator to change the User-Agent of the handshake
	ws.set_option(websocket::stream_base::decorator(
		[](websocket::request_type& req)
		{
			req.set(http::field::user_agent,
			std::string(BOOST_BEAST_VERSION_STRING) +
			" websocket-client-coro");
		}));

	// Perform the websocket handshake
	ws.async_handshake(host, "/", yield[ec]);
	if (ec)
		return fail(ec, "handshake");

	// This buffer will hold the incoming message
	beast::flat_buffer buffer;
	Filter filter;
	MessageDispatcher dispatcher;

	while (true)
	{
		// Read a message into our buffer
		ws.async_read(buffer, yield[ec]);
		if (ec)
		{
			fail(ec, "read");
			continue;
		}
		if (filter.FilterMessage(buffer))
			continue;

		// If we get here then the connection is closed gracefully

		// The make_printable() function helps print a ConstBufferSequence
		std::cout << beast::make_printable(buffer.data()) << std::endl;
		dispatcher.ProcessMessage(buffer);
		dispatcher.SendMessage(ws, yield);

		buffer.clear();
	}

	//// Close the WebSocket connection
	ws.async_close(websocket::close_code::normal, yield[ec]);
	if (ec)
		fail(ec, "close");
}

//------------------------------------------------------------------------------

int main()
{
	// The io_context is required for all I/O
	net::io_context ioc;

	// Launch the asynchronous operation
	boost::asio::spawn(ioc, std::bind(
		&do_session,
		std::string("127.0.0.1"),
		std::string("6700"),
		std::string(""),
		std::ref(ioc),
		std::placeholders::_1));

	// Run the I/O service. The call will return when
	// the socket is closed.
	ioc.run();

	return 0;
}