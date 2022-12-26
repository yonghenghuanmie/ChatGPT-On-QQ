#pragma once

void fail(beast::error_code ec, char const* what);

class Filter
{
public:
	bool FilterMessage(beast::flat_buffer& buffer);

private:
	std::unordered_set<std::string> filter_set{ "lifecycle","heartbeat" };
};

struct Chat
{
	std::size_t group_id;
	std::size_t user_id;
	std::string message;
};

class MessageDispatcher
{
public:
	template<typename TransferProtocol>
	void SendMessage(
		websocket::stream<TransferProtocol>& ws,
		net::yield_context yield
	);

	void ProcessMessage(beast::flat_buffer& buffer);

private:
	std::string AtMessage(std::size_t user_id, std::string&& reply)
	{
		return std::format("[CQ:at,qq={}] {}", user_id, std::move(reply));
	}

	void ParseMessage(json& result)
	{
		std::string group_message = result["message"];
		int begin = 10 + std::to_string(std::size_t(result["self_id"])).size();
		if (group_message.size() > begin &&
			std::string_view(group_message.data(), begin) == "[CQ:at,qq=" + std::to_string(std::size_t(result["self_id"])))
			out_queue.Push(Chat{ result["group_id"],result["user_id"],std::move(std::string(result["message"])).substr(begin) });
	}

	ProducerConsumerQueue<Chat> out_queue;
};