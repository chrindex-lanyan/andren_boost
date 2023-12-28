#include "spl_protocol.h"

#include <algorithm>

namespace chrindex::andren_boost{

const std::string protocol_head = "REQ SPLP/1.1 \r";
const std::string service_head = "Service:";
const std::string request_head = "Request:";
const std::string data_size_head = "data_size:";
const std::string empty_line = "\r";
const std::string next_line = "\n";

void inputor_t::push_data(std::string _data)
{
	std::unique_lock<std::mutex> lock(mut);
	data.push_back(_data);
}

std::string inputor_t::fetch_one()
{
	std::unique_lock<std::mutex> lock(mut);
	if (data.empty())
	{
		return {};
	}
	std::string _data = std::move(data.front());
	data.pop_front();
	return _data;
}

size_t inputor_t::bufCount()
{
	std::unique_lock<std::mutex> lock(mut);
	return data.size();
}

std::string spl_package_t::package(std::string service, std::string request, std::string data)
{
	return std::string() + protocol_head + next_line + service_head + service + next_line + request_head + request + next_line + data_size_head + std::to_string(data.size()) + next_line + empty_line + next_line + data;
}

std::string spl_package_t::package()
{
	data_size = (int64_t)data.size();
	return std::string() + protocol_head + next_line + service_head + service + next_line + request_head + request + next_line + data_size_head + std::to_string(data.size()) + next_line + empty_line + next_line + data;
}

spl_protocol_t::spl_protocol_t() : m_data(std::make_unique<spl_protocol_t::_data_t>())
{
	//
}

spl_protocol_t::~spl_protocol_t()
{
	//
}

void spl_protocol_t::setInputor(std::shared_ptr<inputor_t> input)
{
	m_data->inputor = input;
}

spl_protocol_t::check_type_enum spl_protocol_t::check()
{
	check_type_enum et = check_type_enum::FAILED_NONE;
	std::string buf;

	/// check buffer has new data or not and fetch
	if (!m_data->inputor)
	{
		et = check_type_enum::FAILED_EMPTY_INPUTOR;
		return et;
	}
	buf = m_data->inputor->fetch_one();

	/// check history data
	{
		std::unique_lock<std::mutex> locker(m_data->historyDataMut);
		buf += std::move(m_data->historyData);
	}

	/// check has data
	if (buf.empty())
	{
		return et;
	}

	/// parsing
	struct spl_package_t pack;
	int64_t ret = 0;
	et = parser(buf, pack, ret);

	/// push ready queue ok or not
	if (ret > 0)
	{
		std::unique_lock<std::mutex> locker(m_data->historyDataMut);
		m_data->historyData.clear();
		m_data->historyData = std::string(&buf[ret], buf.size() - ret);
	}
	else if (ret == 0)
	{
		//
	}
	else if ((int32_t)et & int32_t(check_type_enum::OK) || ret < 0)
	{
		if (et == check_type_enum::FAILED_HEAD)
		{
			return et;
		}
		std::unique_lock<std::mutex> locker(m_data->historyDataMut);
		m_data->historyData = std::move(buf);
		return et;
	}

	{
		std::unique_lock<std::mutex> locker(m_data->waitReadQueMut);
		m_data->waitReadQue.push_back(std::move(pack));
	}

	return et;
}

int32_t spl_protocol_t::readyPackageCount()
{
	std::unique_lock<std::mutex> locker(m_data->waitReadQueMut);
	return (int32_t)m_data->waitReadQue.size();
}

spl_package_t spl_protocol_t::fetchOnePackage()
{
	spl_package_t a;
	std::unique_lock<std::mutex> locker(m_data->waitReadQueMut);
	a = std::move(m_data->waitReadQue.front());
	m_data->waitReadQue.pop_front();
	return a;
}

int64_t spl_protocol_t::historyDataSize()
{
	std::unique_lock<std::mutex> locker(m_data->historyDataMut);
	return (int64_t)m_data->historyData.size();
}

std::string spl_protocol_t::historyData()
{
	std::unique_lock<std::mutex> locker(m_data->historyDataMut);
	return m_data->historyData;
}

std::string spl_protocol_t::fetchHistoryData()
{
	std::unique_lock<std::mutex> locker(m_data->historyDataMut);
	return std::move(m_data->historyData);
}

spl_protocol_t::check_type_enum spl_protocol_t::parser(std::string& data, struct spl_package_t& pack, int64_t& ret)
{
	ret = -1;

	check_type_enum et = check_type_enum::FAILED_NONE;

	/// find protocol head
	size_t ops = 0;
	size_t crrt_ops = 0;
	if (((ops = data.find('\n', crrt_ops)) != std::string::npos) && data.substr(crrt_ops, crrt_ops + ops) == protocol_head)
	{
		pack.head = protocol_head;
		crrt_ops = ops + 1; // 1 是因为包含\n
	}
	else
	{
		et = check_type_enum::FAILED_HEAD;
		return et;
	}

	/// find service head
	if ((ops = data.find('\n', crrt_ops)) != std::string::npos)
	{
		size_t divOps = data.find(':', crrt_ops + service_head.size() - 1);
		if (divOps != std::string::npos && data.substr(crrt_ops, divOps + 1 - crrt_ops) == service_head)
		{
			pack.service = data.substr(divOps + 1, ops - divOps - 1);
			crrt_ops = ops + 1;
		}
		else
		{
			et = check_type_enum::FAILED_SERVICE;
			return et;
		}
	}
	else
	{
		et = check_type_enum::FAILED_SERVICE;
		return et;
	}

	/// find request head
	if ((ops = data.find('\n', crrt_ops)) != std::string::npos)
	{
		size_t divOps = data.find(':', crrt_ops + request_head.size() - 1);
		if (divOps != std::string::npos && data.substr(crrt_ops, divOps + 1 - crrt_ops) == request_head)
		{
			pack.request = data.substr(divOps + 1, ops - divOps - 1);
			crrt_ops = ops + 1;
		}
		else
		{
			et = check_type_enum::FAILED_SERVICE;
			return et;
		}
	}
	else
	{
		et = check_type_enum::FAILED_REQUEST;
		return et;
	}

	/// find data size
	if ((ops = data.find('\n', crrt_ops)) != std::string::npos)
	{
		size_t divOps = data.find(':', crrt_ops + data_size_head.size() - 1);
		if (divOps != std::string::npos && data.substr(crrt_ops, divOps + 1 - crrt_ops) == data_size_head)
		{
			try
			{
				pack.data_size = std::stoll(data.substr(divOps + 1, ops - divOps - 1));
			}
			catch (...)
			{
				et = check_type_enum::FAILED_SERVICE;
				return et;
			}
			crrt_ops = ops + 1;
		}
		else
		{
			et = check_type_enum::FAILED_SERVICE;
			return et;
		}
	}
	else
	{
		et = check_type_enum::FAILED_DATA_SIZE;
		return et;
	}

	/// find empty line
	if (((ops = data.find('\n', crrt_ops)) != std::string::npos) && data.substr(crrt_ops, empty_line.size()) == empty_line)
	{
		crrt_ops = ops + 1; // 1 是因为包含\n
	}
	else
	{
		et = check_type_enum::FAILED_EMPTY_LINE;
		return et;
	}

	/// find data
	ret = (int64_t)data.size() - crrt_ops + pack.data_size - 1;
	if (crrt_ops < data.size())
	{
		if (ret < 0) // 数据不够
		{
			et = check_type_enum::FAILED_DATA;
			return et;
		}
		else if (ret == 0) // 数据刚好够
		{
			et = check_type_enum::OK;
		}
		else /// 数据够并且还有剩
		{
			et = (check_type_enum)((int32_t)check_type_enum::OK | (int32_t)check_type_enum::OK_BUT_HAS_MORE_DATA);
		}
	}
	else
	{
		et = check_type_enum::FAILED_DATA;
		return et;
	}

	/// process package
	pack.data.insert(pack.data.begin(), data.begin() + crrt_ops, data.begin() + crrt_ops + pack.data_size);
	ret = crrt_ops + pack.data_size;
	return et;
}

}