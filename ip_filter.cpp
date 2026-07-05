#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <algorithm>

#include "lib.h"

class IpAddress
{
	uint16_t first;
	uint16_t second;
	uint16_t third;
	uint16_t fourth;

public:

	IpAddress()
	{
		this->first = 0;
		this->second = 0;
		this->third = 0;
		this->fourth = 0;
	}

	IpAddress(std::string ip_address)
	{
		sscanf(ip_address.c_str(), "%hu.%hu.%hu.%hu", &this->first, &this->second, &this->third, &this->fourth);
	}

	void print() const
	{
		std::cout << this->first << "." << this->second << "." << this->third << "." << this->fourth << std::endl;
	}

	std::string getIpString() const
	{
		std::ostringstream ip;

		ip << static_cast<unsigned>(first) << '.'
		   << static_cast<unsigned>(second) << '.'
		   << static_cast<unsigned>(third) << '.'
		   << static_cast<unsigned>(fourth);

		return ip.str();
	}


	bool operator==(const IpAddress& other)
	{
		return first == other.getFirst() && second == other.getSecond() && third == other.getThird() && fourth == other.getFourth();
	}

	bool operator<(const IpAddress& other)
	{
		if(first != other.getFirst()) return other.getFirst() < first;
		if(second != other.getSecond()) return other.getSecond() < second;
		if(third != other.getThird()) return other.getThird() < third;
		return other.getFourth() < fourth;
	}

	uint16_t getFirst() const
	{
		return first;
	}


	uint16_t getSecond() const
	{
		return second;
	}

	uint16_t getThird() const
	{
		return third;
	}

	uint16_t getFourth() const
	{
		return fourth;
	}
};

std::string parseIpStr(std::string str)
{
	size_t pos = str.find('\t');
	if (pos == str.npos)
	{
		return str;
	}
	return str.substr(0, pos);
}

bool isFirstByteEqualTo(const IpAddress& ip, uint16_t value)
{
	return ip.getFirst() == value;
}

bool isFirstAndSecondBytesEqualTo(const IpAddress& ip, uint16_t first_value, uint16_t second_value)
{
	return ip.getFirst() == first_value && ip.getSecond() == second_value;
}

bool isAnyByteEqualTo(const IpAddress& ip, uint16_t value)
{
	return ip.getFirst() == value || ip.getSecond() == value || ip.getThird() == value || ip.getFourth() == value;
}

int main(int,char **)
{
	std::string input;
	std::vector<IpAddress> ips;

	std::vector<std::string> ips_cond1;
	std::vector<std::string> ips_cond2;
	std::vector<std::string> ips_cond3;


	while(std::getline(std::cin, input))
	{
		ips.push_back(IpAddress{parseIpStr(input)});
	}

	std::sort(ips.begin(), ips.end());

	for(auto& ip : ips)
	{
		ip.print();

		if (isFirstAndSecondBytesEqualTo(ip,46,70))
		{
			ips_cond2.push_back(ip.getIpString());
		}
		else if (isFirstByteEqualTo(ip,1))
		{
			ips_cond1.push_back(ip.getIpString());
		}


		if (isAnyByteEqualTo(ip,46))
		{
			ips_cond3.push_back(ip.getIpString());
		}
	}

	for(auto& ip : ips_cond1)
	{
		std::cout << ip << std::endl;
	}

	for(auto& ip : ips_cond2)
	{
		std::cout << ip << std::endl;
	}

	for(auto& ip : ips_cond3)
	{
		std::cout << ip << std::endl;
	}

	return 0;
}
